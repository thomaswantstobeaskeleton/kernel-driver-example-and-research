#pragma once
/* DeviceIoControl-based kernel communication - compatible with kdmapper */

#include <ntifs.h>
#include "../flush_comm_config.h"

/* Runtime-built from FLUSHCOMM_SECTION_SEED + suffix (no MdmTrace/WdfCtl literals). Set in FlushComm_Init. */
extern UNICODE_STRING g_flushcomm_reg_path;
#define DEFAULT_MAGGICCODE FLUSHCOMM_MAGIC

#ifndef FILE_DEVICE_BEEP
#define FILE_DEVICE_BEEP 0x00000001
#endif
#define IOCTL_REXCOMM     CTL_CODE(FILE_DEVICE_BEEP, FLUSHCOMM_IOCTL_FUNC, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REXCOMM_PING CTL_CODE(FILE_DEVICE_BEEP, FLUSHCOMM_IOCTL_PING, METHOD_BUFFERED, FILE_ANY_ACCESS)  /* No registry - just write magic to output */

typedef NTSTATUS(*IRP_MJ_DEVICE_CONTROL_HANDLER)(PDEVICE_OBJECT, PIRP);
typedef NTSTATUS(*IRP_MJ_FLUSH_BUFFERS_HANDLER)(PDEVICE_OBJECT, PIRP);
extern IRP_MJ_DEVICE_CONTROL_HANDLER g_orig_device_control;
extern IRP_MJ_FLUSH_BUFFERS_HANDLER g_orig_flush_buffers;

/* Process shared-buffer request (called from IOCTL or IRP_MJ_FLUSH_BUFFERS). Completes the IRP. */
void FlushComm_ProcessSharedBuffer(PDEVICE_OBJECT DeviceObject, PIRP Irp);

/* Process request from g_section_view only (no IRP). Used by ALPC fallback worker thread.
 * Returns STATUS_SUCCESS or error. Caller must attach to g_section_process before calling. */
NTSTATUS FlushComm_ProcessSectionBuffer(void);

/* IRP_MJ_FLUSH_BUFFERS handler - alternative to DeviceIoControl for request signaling */
NTSTATUS FlushComm_FlushHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp);

/* Codecave: when set, PING runs from signed driver space to reduce RIP detection */
extern NTSTATUS(*g_ping_codecave)(PDEVICE_OBJECT, PIRP);

typedef enum _REQUEST_TYPE : ULONG {
    REQ_READ,
    REQ_WRITE,
    // REQ_READ_BATCH removed
    REQ_MAINBASE,
    REQ_GET_DIR_BASE,
    REQ_GET_GUARDED_REGION,
    REQ_MOUSE_MOVE,
    REQ_GET_PID_BY_NAME,
    REQ_INIT = 99,
} REQUEST_TYPE;

typedef struct _REQUEST_DATA {
    ULONG64* MaggicCode;
    ULONG Type;
    PVOID Arguments;
    NTSTATUS* Status;
} REQUEST_DATA, * PREQUEST_DATA;

typedef struct _REQUEST_READ {
    ULONG ProcessId;
    PVOID Dest;
    PVOID Src;
    ULONG Size;
    ULONG bPhysicalMem;  /* 4 bytes to match usermode BOOL layout */
} REQUEST_READ, * PREQUEST_READ;

/* Batch read: args ProcessId+Count; data area: input=Count*(Src8,Size4), output=concatenated results */
typedef struct _REQUEST_READ_BATCH {
    ULONG ProcessId;
    ULONG Count;
} REQUEST_READ_BATCH, * PREQUEST_READ_BATCH;

typedef struct _REQUEST_WRITE {
    ULONG ProcessId;
    PVOID Dest;
    PVOID Src;
    ULONG Size;
    ULONG bPhysicalMem;  /* 4 bytes to match usermode BOOL layout */
} REQUEST_WRITE, * PREQUEST_WRITE;

typedef struct _REQUEST_MAINBASE {
    ULONG ProcessId;
    ULONGLONG* OutAddress;
} REQUEST_MAINBASE, * PREQUEST_MAINBASE;

typedef struct _REQUEST_GET_DIR_BASE {
    ULONG ProcessId;
    ULONGLONG* OutCr3;
    ULONGLONG InBase;  /* Optional: usermode base from find_image for validation when PsGetProcessSectionBaseAddress is spoofed */
} REQUEST_GET_DIR_BASE, * PREQUEST_GET_DIR_BASE;

typedef struct _REQUEST_GET_GUARDED_REGION {
    ULONGLONG* OutAddress;
} REQUEST_GET_GUARDED_REGION, * PREQUEST_GET_GUARDED_REGION;

typedef struct _REQUEST_MOUSE_MOVE {
    LONG DeltaX;
    LONG DeltaY;
    USHORT ButtonFlags;
} REQUEST_MOUSE_MOVE, * PREQUEST_MOUSE_MOVE;

/* Input: Name (ImageFileName, up to 15 chars + null). Output: section path = data area; registry path = OutPid. */
#define REQUEST_GET_PID_BY_NAME_NAMELEN 16
typedef struct _REQUEST_GET_PID_BY_NAME {
    CHAR Name[REQUEST_GET_PID_BY_NAME_NAMELEN];
    ULONG OutPid;  /* written by driver when using Arguments (registry path) */
} REQUEST_GET_PID_BY_NAME, * PREQUEST_GET_PID_BY_NAME;

NTSTATUS RegReadQword(PCUNICODE_STRING path, PCUNICODE_STRING valueName, PULONG64 outVal);
bool RegWriteQword(PCUNICODE_STRING path, PCUNICODE_STRING valueName, PVOID data, ULONG size);
/* DriverObject required when FLUSHCOMM_USE_FILEOBJ_HOOK; otherwise can be NULL */
NTSTATUS FlushComm_Init(PDRIVER_OBJECT DriverObject);

/* Section-based shared memory (FLUSHCOMM_USE_SECTION): no MmCopyVirtualMemory */
#if FLUSHCOMM_USE_SECTION
extern PVOID g_section_view;
extern HANDLE g_section_handle;
extern PEPROCESS g_section_process;  /* System process - attach before accessing section view */
/* FLUSHCOMM_DATA_OFFSET / FLUSHCOMM_STATUS_OFFSET from flush_comm_config.h */
#define FLUSHCOMM_DATA_SIZE   (FLUSHCOMM_SECTION_SIZE - FLUSHCOMM_DATA_OFFSET)
#endif
