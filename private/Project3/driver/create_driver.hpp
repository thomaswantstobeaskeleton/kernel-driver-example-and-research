#pragma once
/* Th3Spl-style IoCreateDriver: ObCreateObject + ObInsertObject path.
 * Bypasses PsLoadedModuleList and EtwTiLogDriverObjectLoad - no ETW driver-load telemetry.
 * Uses random driver name (\Driver\%08X) to reduce enumeration detection.
 * See: github.com/Th3Spl/IoCreateDriver */

#include <ntifs.h>
#include <wdm.h>
#include <ntstrsafe.h>
#include "page_evasion.hpp"
#include "includes.hpp"
#include "../flush_comm_config.h"
#include "routine_obfuscate.h"

/* Pool tag built from bytes - no literal 'wDfc' in .rdata (WDF-like) */
static inline ULONG create_driver_pool_tag(void) {
    return (ULONG)(UCHAR)'w' | ((ULONG)(UCHAR)'D'<<8) | ((ULONG)(UCHAR)'f'<<16) | ((ULONG)(UCHAR)'c'<<24);
}
#define CREATE_DRIVER_POOL_TAG  create_driver_pool_tag()

typedef NTSTATUS(NTAPI* PDRIVER_INIT)(PDRIVER_OBJECT, PUNICODE_STRING);

static NTSTATUS CreateDriver_InvalidRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}

/* Th3Spl-style: ObCreateObject + ObInsertObject - no PsLoadedModuleList, no EtwTiLogDriverObjectLoad */
static NTSTATUS CreateDriver_ObCreatePath(PDRIVER_INIT EntryPoint) {
    typedef NTSTATUS(NTAPI* ObCreateObject_t)(KPROCESSOR_MODE, POBJECT_TYPE, POBJECT_ATTRIBUTES,
        KPROCESSOR_MODE, PVOID, ULONG, ULONG, ULONG, PVOID*);
    typedef NTSTATUS(NTAPI* ObInsertObject_t)(PVOID, PVOID, ULONG, ULONG*, PVOID, PVOID*);
    typedef VOID(NTAPI* ObMakeTemporaryObject_t)(PVOID);

    static ObCreateObject_t pObCreateObject = nullptr;
    static ObInsertObject_t pObInsertObject = nullptr;
    static ObMakeTemporaryObject_t pObMakeTemporaryObject = nullptr;

    if (!pObCreateObject) {
        pObCreateObject = (ObCreateObject_t)get_system_routine_obf(OBF_ObCreateObject, sizeof(OBF_ObCreateObject));
        pObInsertObject = (ObInsertObject_t)get_system_routine_obf(OBF_ObInsertObject, sizeof(OBF_ObInsertObject));
        pObMakeTemporaryObject = (ObMakeTemporaryObject_t)get_system_routine_obf(OBF_ObMakeTemporaryObject, sizeof(OBF_ObMakeTemporaryObject));
        if (!pObCreateObject || !pObInsertObject || !pObMakeTemporaryObject)
            return STATUS_NOT_FOUND;
    }

    /* IoDriverObjectType: MmGetSystemRoutineAddress does not resolve data symbols; use linked import */
    POBJECT_TYPE driverType = *globals::IoDriverObjectType;
    if (!driverType)
        return STATUS_NOT_FOUND;

    WCHAR namePrefix[16];
    routine_obf_decode_to_wide(OBF_DriverNamePrefix, sizeof(OBF_DriverNamePrefix), namePrefix, 16);
    WCHAR nameBuf[64];
    if (!NT_SUCCESS(RtlStringCbPrintfW(nameBuf, sizeof(nameBuf), L"%ws%08X",
        namePrefix, (ULONG)(KeQueryUnbiasedInterruptTime() & 0xFFFFFFFF))))
        return STATUS_UNSUCCESSFUL;

    UNICODE_STRING localName;
    RtlInitUnicodeString(&localName, nameBuf);

    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &localName, OBJ_PERMANENT | OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    ULONG objSize = sizeof(DRIVER_OBJECT) + sizeof(DRIVER_EXTENSION);
    PDRIVER_OBJECT drvObj = nullptr;

    NTSTATUS status = pObCreateObject(KernelMode, driverType, &oa, KernelMode, NULL,
        objSize, 0, 0, (PVOID*)&drvObj);
    if (!NT_SUCCESS(status) || !drvObj)
        return status;

    RtlZeroMemory(drvObj, objSize);
    drvObj->Type = IO_TYPE_DRIVER;
    drvObj->Size = sizeof(DRIVER_OBJECT);
    drvObj->Flags = DRVO_BUILTIN_DRIVER;
    drvObj->DriverExtension = (PDRIVER_EXTENSION)((PUCHAR)drvObj + sizeof(DRIVER_OBJECT));
    drvObj->DriverExtension->DriverObject = drvObj;
    drvObj->DriverInit = (PDRIVER_INITIALIZE)EntryPoint;

    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        drvObj->MajorFunction[i] = CreateDriver_InvalidRequest;

    SIZE_T allocSize = localName.MaximumLength;
    PWCH svcBuf = (PWCH)ExAllocatePoolWithTag(PagedPool, allocSize, CREATE_DRIVER_POOL_TAG);
    if (!svcBuf) {
        pObMakeTemporaryObject(drvObj);
        ObfDereferenceObject(drvObj);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory(svcBuf, localName.Buffer, localName.Length);
    svcBuf[localName.Length / sizeof(WCHAR)] = L'\0';
    drvObj->DriverExtension->ServiceKeyName.Buffer = svcBuf;
    drvObj->DriverExtension->ServiceKeyName.Length = localName.Length;
    drvObj->DriverExtension->ServiceKeyName.MaximumLength = (USHORT)allocSize;

    PWCH nameBuf2 = (PWCH)ExAllocatePoolWithTag(PagedPool, localName.MaximumLength, CREATE_DRIVER_POOL_TAG);
    if (!nameBuf2) {
        ExFreePoolWithTag(svcBuf, CREATE_DRIVER_POOL_TAG);
        pObMakeTemporaryObject(drvObj);
        ObfDereferenceObject(drvObj);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory(nameBuf2, localName.Buffer, localName.Length);
    nameBuf2[localName.Length / sizeof(WCHAR)] = L'\0';
    drvObj->DriverName.Buffer = nameBuf2;
    drvObj->DriverName.Length = localName.Length;
    drvObj->DriverName.MaximumLength = (USHORT)localName.MaximumLength;

    HANDLE hObj = nullptr;
    status = pObInsertObject(drvObj, NULL, FILE_READ_DATA, 0, NULL, &hObj);
    if (hObj) ZwClose(hObj);
    if (!NT_SUCCESS(status)) {
        /* ObInsertObject automatically dereferences on failure - do NOT call ObMakeTemporaryObject/ObfDereferenceObject */
        ExFreePoolWithTag(svcBuf, CREATE_DRIVER_POOL_TAG);
        ExFreePoolWithTag(nameBuf2, CREATE_DRIVER_POOL_TAG);
        return status;
    }

    status = EntryPoint(drvObj, NULL);
    if (!NT_SUCCESS(status)) {
        pObMakeTemporaryObject(drvObj);
        ObfDereferenceObject(drvObj);
        return status;
    }

    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        if (!drvObj->MajorFunction[i])
            drvObj->MajorFunction[i] = CreateDriver_InvalidRequest;
    }
    return status;
}
