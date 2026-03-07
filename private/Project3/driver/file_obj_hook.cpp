#include "file_obj_hook.hpp"
#include "flush_comm.hpp"
#include "defines.h"
#include "includes.hpp"
#include "../flush_comm_config.h"

#if FLUSHCOMM_USE_FILEOBJ_HOOK

#define message(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[FileObjHook] " __VA_ARGS__)

static PDEVICE_OBJECT g_fake_device = nullptr;
static PDRIVER_OBJECT g_our_driver = nullptr;
static PDEVICE_OBJECT g_target_device = nullptr;
static PDRIVER_OBJECT g_target_driver = nullptr;
static NTSTATUS(*g_orig_create)(PDEVICE_OBJECT, PIRP) = nullptr;
static FileObjHook_DeviceControl_t g_device_control_handler = nullptr;
static FileObjHook_FlushBuffers_t g_flush_buffers_handler = nullptr;

/* Router: when IRP targets our fake device, dispatch to FlushComm; else pass through */
static NTSTATUS InvalidDeviceRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS Hook_Create(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    return g_orig_create ? g_orig_create(DeviceObject, Irp) : InvalidDeviceRequest(DeviceObject, Irp);
}

NTSTATUS FileObjHook_CompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context) {
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Context);
    if (Irp->PendingReturned)
        IoMarkIrpPending(Irp);

    if (NT_SUCCESS(Irp->IoStatus.Status) && g_fake_device) {
        PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
        PFILE_OBJECT fileObj = irpSp->FileObject;
        if (fileObj && fileObj->DeviceObject == g_target_device) {
            fileObj->DeviceObject = g_fake_device;
            message("Redirected FILE_OBJECT to fake device\n");
        }
    }
    return STATUS_SUCCESS;
}

static NTSTATUS Hook_CreateWithCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (!g_orig_create)
        return InvalidDeviceRequest(DeviceObject, Irp);

    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, FileObjHook_CompletionRoutine, nullptr, TRUE, TRUE, TRUE);
    return g_orig_create(DeviceObject, Irp);
}

void FileObjHook_SetHandlers(FileObjHook_DeviceControl_t deviceControl, FileObjHook_FlushBuffers_t flushBuffers) {
    g_device_control_handler = deviceControl;
    g_flush_buffers_handler = flushBuffers;
}

NTSTATUS FileObjHook_Init(PDEVICE_OBJECT pTargetDevice, PDRIVER_OBJECT pTargetDriver) {
    if (!pTargetDevice || !pTargetDriver || !g_our_driver)
        return STATUS_INVALID_PARAMETER;

    g_target_device = pTargetDevice;
    g_target_driver = pTargetDriver;

    NTSTATUS status = IoCreateDevice(g_our_driver, 0, nullptr, FILE_DEVICE_BEEP,
        FILE_DEVICE_SECURE_OPEN, FALSE, &g_fake_device);
    if (!NT_SUCCESS(status)) {
        message("IoCreateDevice failed: 0x%X\n", status);
        return status;
    }

    g_fake_device->Flags |= DO_BUFFERED_IO;
    g_fake_device->Flags &= ~DO_DEVICE_INITIALIZING;

    PVOID* pCreate = (PVOID*)&pTargetDriver->MajorFunction[IRP_MJ_CREATE];
    g_orig_create = (NTSTATUS(*)(PDEVICE_OBJECT, PIRP))InterlockedExchangePointer(pCreate, Hook_CreateWithCompletion);

    message("FILE_OBJECT hook installed (create completion)\n");
    return STATUS_SUCCESS;
}

void FileObjHook_Shutdown(void) {
    if (g_target_driver && g_orig_create) {
        PVOID* pCreate = (PVOID*)&g_target_driver->MajorFunction[IRP_MJ_CREATE];
        InterlockedExchangePointer(pCreate, g_orig_create);
        g_orig_create = nullptr;
    }
    if (g_fake_device) {
        IoDeleteDevice(g_fake_device);
        g_fake_device = nullptr;
    }
    g_target_device = nullptr;
    g_target_driver = nullptr;
    message("FILE_OBJECT hook removed\n");
}

void FileObjHook_SetOurDriver(PDRIVER_OBJECT pDrv) {
    g_our_driver = pDrv;
}

#endif
