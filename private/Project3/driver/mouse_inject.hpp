#pragma once
#include "page_evasion.hpp"
/* MouHidInputHook-style CONNECT_DATA resolution - most stealth kernel mouse injection.
 * - No filter device attached (no device stack modification)
 * - CONNECT_DATA offset not in public headers (heuristic resolution)
 * - Validates ClassService is in MouClass executable (fewer false positives)
 * - Anti-detection: worker thread, delay jitter, rate limiting.
 * Based on changeofpace/MouHidInputHook, MouClassInputInjection. */

#include <ntifs.h>
#include <ntddmou.h>
#include <ntimage.h>
#include "defines.h"
#include "includes.hpp"
#include "routine_obfuscate.h"

/* CONNECT_DATA from MouHid - matches kbdmou but avoids PSERVICE_CALLBACK_ROUTINE conflict with defines.h */
typedef struct _MOUSE_CONNECT_DATA {
    PDEVICE_OBJECT ClassDeviceObject;
    PVOID ClassService;
} MOUSE_CONNECT_DATA, *PMOUSE_CONNECT_DATA;

namespace mouse_inject
{
    inline PDEVICE_OBJECT g_mouse_device = nullptr;
    inline PSERVICE_CALLBACK_ROUTINE g_service_callback = nullptr;

    constexpr LONGLONG MIN_MOVE_INTERVAL_100NS = 50000;  /* 5ms - human-like rate limit */
    constexpr ULONG JITTER_MAX_MS = 2;
    constexpr SIZE_T DEVICE_EXTENSION_SEARCH_SIZE = 0x100;

    inline LARGE_INTEGER g_last_move_time = { 0 };
    inline PIO_WORKITEM g_work_item = nullptr;

    struct MOUSE_WORK_CTX {
        LONG DeltaX;
        LONG DeltaY;
        USHORT ButtonFlags;
    };

    /* Check if addr is within executable sections of image at ImageBase */
    __forceinline static bool is_in_executable_sections(ULONG_PTR ImageBase, ULONG_PTR addr)
    {
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)ImageBase;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(ImageBase + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        ULONG nSections = nt->FileHeader.NumberOfSections;
        PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
        for (ULONG i = 0; i < nSections; i++, sec++) {
            if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                ULONG_PTR secStart = ImageBase + sec->VirtualAddress;
                ULONG_PTR secEnd = secStart + sec->Misc.VirtualSize;
                if (addr >= secStart && addr < secEnd)
                    return true;
            }
        }
        return false;
    }

    /* MouHidInputHook heuristic: iterate MouHid devices, validate CONNECT_DATA via executable section check */
    __forceinline bool resolve_mouse_stack()
    {
        if (g_mouse_device && g_service_callback)
            return true;

        WCHAR hid_name_buf[24];
        routine_obf_decode_to_wide(OBF_MouHID, sizeof(OBF_MouHID), hid_name_buf, 24);
        UNICODE_STRING hid_name;
        RtlInitUnicodeString(&hid_name, hid_name_buf);
        PDRIVER_OBJECT hid_drv = nullptr;
        NTSTATUS st = ObReferenceObjectByName(&hid_name, OBJ_CASE_INSENSITIVE, nullptr, 0,
            *globals::IoDriverObjectType, KernelMode, nullptr, (PVOID*)&hid_drv);
        if (!NT_SUCCESS(st) || !hid_drv) return false;

        /* Enumerate MouHid devices */
        ULONG nDevices = 0;
        st = IoEnumerateDeviceObjectList(hid_drv, nullptr, 0, &nDevices);
        if (st != STATUS_BUFFER_TOO_SMALL || nDevices == 0) {
            ObDereferenceObject(hid_drv);
            return false;
        }
        PDEVICE_OBJECT* devList = (PDEVICE_OBJECT*)ExAllocatePoolWithTag(NonPagedPool, nDevices * sizeof(PDEVICE_OBJECT), EVASION_POOL_TAG_LIST_R);
        if (!devList) {
            ObDereferenceObject(hid_drv);
            return false;
        }
        st = IoEnumerateDeviceObjectList(hid_drv, devList, nDevices * sizeof(PDEVICE_OBJECT), &nDevices);
        if (!NT_SUCCESS(st)) {
            ExFreePoolWithTag(devList, EVASION_POOL_TAG_LIST_R);
            ObDereferenceObject(hid_drv);
            return false;
        }

        g_mouse_device = nullptr;
        g_service_callback = nullptr;

        /* Get MouClass driver for validation (ClassService must be in mouclass.sys) */
        WCHAR class_name_buf[24];
        routine_obf_decode_to_wide(OBF_MouClass, sizeof(OBF_MouClass), class_name_buf, 24);
        UNICODE_STRING class_name;
        RtlInitUnicodeString(&class_name, class_name_buf);
        PDRIVER_OBJECT class_drv = nullptr;
        st = ObReferenceObjectByName(&class_name, OBJ_CASE_INSENSITIVE, nullptr, 0,
            *globals::IoDriverObjectType, KernelMode, nullptr, (PVOID*)&class_drv);
        if (!NT_SUCCESS(st) || !class_drv || !class_drv->DriverStart) {
            for (ULONG i = 0; i < nDevices; i++)
                if (devList[i]) ObDereferenceObject(devList[i]);
            ExFreePoolWithTag(devList, EVASION_POOL_TAG_LIST_R);
            ObDereferenceObject(hid_drv);
            return false;
        }
        ULONG_PTR mouclass_base = (ULONG_PTR)class_drv->DriverStart;

        for (ULONG i = 0; i < nDevices && !g_service_callback; i++) {
            PDEVICE_OBJECT mouhid_dev = devList[i];
            if (!mouhid_dev || !mouhid_dev->DeviceExtension) continue;

            /* Scan device extension for CONNECT_DATA (pointer-aligned) */
            PUCHAR ext = (PUCHAR)mouhid_dev->DeviceExtension;
            ULONG_PTR searchEnd = (ULONG_PTR)ext + DEVICE_EXTENSION_SEARCH_SIZE;
            if (searchEnd > (ULONG_PTR)ext + PAGE_SIZE)
                searchEnd = (ULONG_PTR)PAGE_ALIGN(ext + PAGE_SIZE);

            for (PMOUSE_CONNECT_DATA pcd = (PMOUSE_CONNECT_DATA)ext;
                 (ULONG_PTR)pcd + sizeof(MOUSE_CONNECT_DATA) <= searchEnd;
                 pcd = (PMOUSE_CONNECT_DATA)((ULONG_PTR)pcd + sizeof(ULONG_PTR))) {

                __try {
                    if (!pcd->ClassDeviceObject || !pcd->ClassService) continue;
                    /* ClassDeviceObject must be a MouClass device */
                    if (pcd->ClassDeviceObject->DriverObject != class_drv) continue;
                    /* ClassService must be in MouClass executable (MouHidInputHook validation) */
                    if (!is_in_executable_sections(mouclass_base, (ULONG_PTR)pcd->ClassService))
                        continue;
                } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }

                g_mouse_device = pcd->ClassDeviceObject;
                g_service_callback = (PSERVICE_CALLBACK_ROUTINE)pcd->ClassService;
                break;
            }
        }

        ObDereferenceObject(class_drv);

        for (ULONG i = 0; i < nDevices; i++)
            if (devList[i]) ObDereferenceObject(devList[i]);
        ExFreePoolWithTag(devList, EVASION_POOL_TAG_LIST_R);
        ObDereferenceObject(hid_drv);

        if (g_mouse_device && g_service_callback)
            return true;

        if (!g_mouse_device) {
            /* Fallback: use original vsaint1-style scan (MouClass+MouHID walk) */
            routine_obf_decode_to_wide(OBF_MouClass, sizeof(OBF_MouClass), class_name_buf, 24);
            RtlInitUnicodeString(&class_name, class_name_buf);
            PDRIVER_OBJECT class_drv = nullptr;
            st = ObReferenceObjectByName(&class_name, OBJ_CASE_INSENSITIVE, nullptr, 0,
                *globals::IoDriverObjectType, KernelMode, nullptr, (PVOID*)&class_drv);
            if (!NT_SUCCESS(st) || !class_drv) return false;

            routine_obf_decode_to_wide(OBF_MouHID, sizeof(OBF_MouHID), hid_name_buf, 24);
            RtlInitUnicodeString(&hid_name, hid_name_buf);
            st = ObReferenceObjectByName(&hid_name, OBJ_CASE_INSENSITIVE, nullptr, 0,
                *globals::IoDriverObjectType, KernelMode, nullptr, (PVOID*)&hid_drv);
            if (!NT_SUCCESS(st) || !hid_drv) {
                ObDereferenceObject(class_drv);
                return false;
            }

            PVOID class_base = class_drv->DriverStart;
            PDEVICE_OBJECT hid_dev = hid_drv->DeviceObject;
            while (hid_dev && !g_service_callback) {
                PDEVICE_OBJECT class_dev = class_drv->DeviceObject;
                while (class_dev && !g_service_callback) {
                    PULONG_PTR ext = (PULONG_PTR)hid_dev->DeviceExtension;
                    for (SIZE_T j = 0; j < 64; j++) {
                        if (ext[j] == (ULONG_PTR)class_dev && ext[j + 1] > (ULONG_PTR)class_base) {
                            g_mouse_device = class_dev;  /* ClassDeviceObject from CONNECT_DATA */
                            g_service_callback = (PSERVICE_CALLBACK_ROUTINE)ext[j + 1];
                            break;
                        }
                    }
                    class_dev = class_dev->NextDevice;
                }
                hid_dev = hid_dev->AttachedDevice;
            }
            ObDereferenceObject(class_drv);
            ObDereferenceObject(hid_drv);
        }

        return (g_mouse_device && g_service_callback);
    }

    __forceinline void do_move(long x, long y, USHORT button_flags)
    {
        if (!resolve_mouse_stack()) return;
        MOUSE_INPUT_DATA mid = { 0 };
        mid.LastX = (LONG)x;
        mid.LastY = (LONG)y;
        mid.ButtonFlags = button_flags;
        mid.UnitId = 1;
        ULONG consumed = 0;
        KIRQL old;
        KeRaiseIrql(DISPATCH_LEVEL, &old);
        g_service_callback(g_mouse_device, &mid, (PMOUSE_INPUT_DATA)&mid + 1, &consumed);
        KeLowerIrql(old);
    }

    void mouse_work_routine(PDEVICE_OBJECT DeviceObject, PVOID Context);

    __forceinline void move_async(PDEVICE_OBJECT DeviceObject, long x, long y, USHORT button_flags)
    {
        if (!DeviceObject) return;
        if (!g_work_item)
            g_work_item = IoAllocateWorkItem(DeviceObject);
        if (!g_work_item) {
            do_move(x, y, button_flags);
            return;
        }
        MOUSE_WORK_CTX* ctx = (MOUSE_WORK_CTX*)ExAllocatePoolWithTag(NonPagedPool, sizeof(MOUSE_WORK_CTX), EVASION_POOL_TAG_WORK_R);
        if (!ctx) {
            do_move(x, y, button_flags);
            return;
        }
        ctx->DeltaX = (LONG)x;
        ctx->DeltaY = (LONG)y;
        ctx->ButtonFlags = button_flags;
        IoQueueWorkItem(g_work_item, mouse_work_routine, DelayedWorkQueue, ctx);
    }

    __forceinline void move(long x, long y, USHORT button_flags)
    {
        do_move(x, y, button_flags);
    }
}
