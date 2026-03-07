#pragma once
/* FILE_OBJECT DeviceObject redirection - Spectre-style.
 * Replaces DeviceObject in FILE_OBJECT with fake device; real MajorFunction untouched.
 * See IOCTL_ALTERNATIVES_RESEARCH.md. */

#include <ntifs.h>
#include "../flush_comm_config.h"

#if FLUSHCOMM_USE_FILEOBJ_HOOK

/* Initialize: create fake device/driver, hook IRP_MJ_CREATE on target to redirect FILE_OBJECTs.
 * pTargetDevice = device to intercept (e.g. Beep's device).
 * pTargetDriver = its driver (for DriverName, etc).
 * Returns STATUS_SUCCESS if hook installed. */
NTSTATUS FileObjHook_Init(PDEVICE_OBJECT pTargetDevice, PDRIVER_OBJECT pTargetDriver);

/* Unhook and free fake objects. */
void FileObjHook_Shutdown(void);

/* Handler type - our FlushComm handlers. */
typedef NTSTATUS(*FileObjHook_DeviceControl_t)(PDEVICE_OBJECT, PIRP);
typedef NTSTATUS(*FileObjHook_FlushBuffers_t)(PDEVICE_OBJECT, PIRP);
void FileObjHook_SetHandlers(FileObjHook_DeviceControl_t deviceControl, FileObjHook_FlushBuffers_t flushBuffers);

/* Must be called before Init - provides our driver for IoCreateDevice. */
void FileObjHook_SetOurDriver(PDRIVER_OBJECT pDrv);

#endif
