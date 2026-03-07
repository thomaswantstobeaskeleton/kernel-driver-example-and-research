#include "wsk_server.hpp"
#include "flush_comm.hpp"
#include "defines.h"
#include "../flush_comm_config.h"
#include "routine_obfuscate.h"

#if FLUSHCOMM_USE_WSK

#include <wsk.h>

#define message(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[WSK] " __VA_ARGS__)

static const USHORT DEFAULT_WSK_PORT = 41891;
static volatile bool g_wsk_active = false;
static USHORT g_wsk_port = 0;

typedef NTSTATUS(NTAPI* WskRegister_t)(PWSK_CLIENT_NPI, PWSK_REGISTRATION);
typedef NTSTATUS(NTAPI* WskDeregister_t)(PWSK_REGISTRATION);
typedef NTSTATUS(NTAPI* WskCaptureProviderNPI_t)(PWSK_REGISTRATION, ULONG, PWSK_PROVIDER_NPI);
typedef void(NTAPI* WskReleaseProviderNPI_t)(PWSK_REGISTRATION);

static WskRegister_t g_WskRegister = nullptr;
static WskDeregister_t g_WskDeregister = nullptr;
static WskCaptureProviderNPI_t g_WskCaptureProviderNPI = nullptr;
static WskReleaseProviderNPI_t g_WskReleaseProviderNPI = nullptr;

static WSK_REGISTRATION g_wsk_reg = { 0 };
static WSK_PROVIDER_NPI g_wsk_provider = { 0 };
static PWSK_SOCKET g_listen_socket = nullptr;
static PWSK_CLIENT g_wsk_client = nullptr;

static NTSTATUS WskClientEvent(PVOID Context, ULONG EventType, PVOID Information, SIZE_T InformationLength) {
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(EventType);
    UNREFERENCED_PARAMETER(Information);
    UNREFERENCED_PARAMETER(InformationLength);
    return STATUS_SUCCESS;
}

static NTSTATUS WskAcceptEvent(PVOID SocketContext, ULONG Flags,
    PSOCKADDR LocalAddress, PSOCKADDR RemoteAddress,
    PWSK_SOCKET AcceptSocket, PVOID* AcceptSocketContext,
    CONST WSK_CLIENT_CONNECTION_DISPATCH** AcceptSocketDispatch) {
    UNREFERENCED_PARAMETER(SocketContext);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(LocalAddress);
    UNREFERENCED_PARAMETER(RemoteAddress);
    if (AcceptSocket) {
        *AcceptSocketContext = nullptr;
        *AcceptSocketDispatch = nullptr;
        /* Skeleton: accept only - full impl would process FlushComm requests on socket */
    }
    return STATUS_SUCCESS;
}

static WSK_CLIENT_DISPATCH g_wsk_client_dispatch = {
    MAKE_WSK_VERSION(1, 0),
    0,
    WskClientEvent
};

static WSK_CLIENT_LISTEN_DISPATCH g_listen_dispatch = {
    WskAcceptEvent,
    nullptr,
    nullptr
};

static NTSTATUS WskCompletionSync(PDEVICE_OBJECT, PIRP Irp, PVOID Ctx) {
    UNREFERENCED_PARAMETER(Irp);
    if (Ctx) KeSetEvent((PKEVENT)Ctx, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

static bool ResolveWskFunctions(void) {
    if (g_WskRegister) return true;
    g_WskRegister = (WskRegister_t)get_system_routine_obf(OBF_WskRegister, sizeof(OBF_WskRegister));
    g_WskDeregister = (WskDeregister_t)get_system_routine_obf(OBF_WskDeregister, sizeof(OBF_WskDeregister));
    g_WskCaptureProviderNPI = (WskCaptureProviderNPI_t)get_system_routine_obf(OBF_WskCaptureProviderNPI, sizeof(OBF_WskCaptureProviderNPI));
    g_WskReleaseProviderNPI = (WskReleaseProviderNPI_t)get_system_routine_obf(OBF_WskReleaseProviderNPI, sizeof(OBF_WskReleaseProviderNPI));
    if (!g_WskRegister || !g_WskDeregister || !g_WskCaptureProviderNPI || !g_WskReleaseProviderNPI) {
        message("WSK functions not found (netio.sys may not be loaded)\n");
        return false;
    }
    return true;
}

NTSTATUS WskServer_Init(USHORT port) {
    if (g_wsk_active) return STATUS_SUCCESS;

    if (!ResolveWskFunctions())
        return STATUS_NOT_FOUND;

    g_wsk_port = (port != 0) ? port : (FLUSHCOMM_WSK_PORT != 0 ? (USHORT)FLUSHCOMM_WSK_PORT : DEFAULT_WSK_PORT);

    WSK_CLIENT_NPI clientNpi = { 0 };
    clientNpi.ClientContext = nullptr;
    clientNpi.Dispatch = &g_wsk_client_dispatch;

    NTSTATUS status = g_WskRegister(&clientNpi, &g_wsk_reg);
    if (!NT_SUCCESS(status)) {
        message("WskRegister failed: 0x%X\n", status);
        return status;
    }

    status = g_WskCaptureProviderNPI(&g_wsk_reg, 0, &g_wsk_provider);
    if (!NT_SUCCESS(status)) {
        g_WskDeregister(&g_wsk_reg);
        message("WskCaptureProviderNPI failed: 0x%X\n", status);
        return status;
    }

    g_wsk_client = g_wsk_provider.Client;

    PIRP irp = IoAllocateIrp(1, FALSE);
    if (!irp) {
        g_WskReleaseProviderNPI(&g_wsk_reg);
        g_WskDeregister(&g_wsk_reg);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    PWSK_PROVIDER_DISPATCH provDisp = (PWSK_PROVIDER_DISPATCH)g_wsk_provider.Dispatch;
    PFN_WSK_SOCKET WskSocket = provDisp->WskSocket;
    if (!WskSocket) {
        IoFreeIrp(irp);
        g_WskReleaseProviderNPI(&g_wsk_reg);
        g_WskDeregister(&g_wsk_reg);
        return STATUS_NOT_SUPPORTED;
    }

    KEVENT kev;
    KeInitializeEvent(&kev, SynchronizationEvent, FALSE);
    IoSetCompletionRoutine(irp, WskCompletionSync, &kev, TRUE, TRUE, TRUE);

    status = WskSocket(g_wsk_client, AF_INET, SOCK_STREAM, IPPROTO_TCP,
        WSK_FLAG_LISTEN_SOCKET, nullptr, &g_listen_dispatch,
        nullptr, nullptr, nullptr, irp);

    if (status == STATUS_PENDING)
        KeWaitForSingleObject(&kev, Executive, KernelMode, FALSE, nullptr);

    status = irp->IoStatus.Status;
    if (NT_SUCCESS(status))
        g_listen_socket = (PWSK_SOCKET)irp->IoStatus.Information;
    IoFreeIrp(irp);

    if (!NT_SUCCESS(status)) {
        message("WskSocket (listen) failed: 0x%X\n", status);
        g_WskReleaseProviderNPI(&g_wsk_reg);
        g_WskDeregister(&g_wsk_reg);
        return status;
    }
    (void)g_listen_socket; /* TODO: bind, listen - skeleton */

    g_wsk_active = true;
    message("WSK server initialized (port %u) - skeleton\n", g_wsk_port);
    return STATUS_SUCCESS;
}

void WskServer_Shutdown(void) {
    if (!g_wsk_active) return;
    g_wsk_active = false;
    if (g_listen_socket && g_wsk_provider.Dispatch) {
        PWSK_PROVIDER_LISTEN_DISPATCH d = (PWSK_PROVIDER_LISTEN_DISPATCH)g_wsk_provider.Dispatch;
        if (d->Basic.WskCloseSocket) {
            PIRP irp = IoAllocateIrp(1, FALSE);
            if (irp) d->Basic.WskCloseSocket(g_listen_socket, irp);
        }
        g_listen_socket = nullptr;
    }
    g_WskReleaseProviderNPI(&g_wsk_reg);
    g_WskDeregister(&g_wsk_reg);
    message("WSK server shutdown\n");
}

bool WskServer_IsActive(void) {
    return g_wsk_active;
}

USHORT WskServer_GetPort(void) {
    return g_wsk_port;
}

#endif
