#pragma once
/* ALPC fallback for driver comm - used only when FlushFileBuffers handshake fails.
 * Unique design: signal-only ALPC + existing section for data. See ALPC_UNIQUE_DESIGN.md.
 * No implementation copied from public sources; port name derived from FLUSHCOMM_MAGIC. */

#if !FLUSHCOMM_USE_ALPC_FALLBACK
#error "alpc_fallback.hpp should only be included when FLUSHCOMM_USE_ALPC_FALLBACK is 1"
#endif

#include <Windows.h>
#include <winternl.h>
#include "../api_resolve.hpp"
#include "../../flush_comm_config.h"

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) ((((NTSTATUS)(s)) >= 0))
#endif

/* Minimal message for ALPC signal (layout compatible with PORT_MESSAGE header + 1 byte data) */
#pragma pack(push, 1)
struct ALPC_MSG {
    ULONG u1;
    ULONG u2;
    ULONG_PTR u3[2];
    ULONG msgid;
    ULONG datalen;
    UCHAR data[16];
};
#pragma pack(pop)

/* Build port name from FLUSHCOMM_MAGIC so it is unique per build and not a literal spread online */
static void alpc_port_name(wchar_t* out, size_t out_len) {
    const ULONG32 lo = (ULONG32)(FLUSHCOMM_MAGIC & 0xFFFFFFFFu);
    wcscpy_s(out, out_len, L"\\RPC Control\\Svc");
    wchar_t* p = out + wcslen(out);
    for (int i = 7; i >= 0; i--) {
        UINT nib = (lo >> (i * 4)) & 0xF;
        *p++ = (nib < 10) ? (L'0' + nib) : (L'a' + nib - 10);
    }
    *p = L'\0';
}

/* Try connect to driver ALPC port and handshake. Returns true if ALPC path is active. */
static bool try_alpc_connect() {
    if (!g_shared_buf || !g_section_handle) return false;
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    if (!ntdll) return false;
    typedef NTSTATUS(NTAPI* NtAlpcConnectPort_t)(PHANDLE, PUNICODE_STRING, POBJECT_ATTRIBUTES, PVOID, ULONG, PSID, PVOID, PULONG, PVOID, PVOID, PLARGE_INTEGER);
    typedef NTSTATUS(NTAPI* NtAlpcSendWaitReceivePort_t)(HANDLE, ULONG, PVOID, PVOID, PVOID, PULONG, PVOID, PLARGE_INTEGER);
    auto NtAlpcConnectPort = (NtAlpcConnectPort_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtAlpcConnectPort"));
    auto NtAlpcSendWaitReceivePort = (NtAlpcSendWaitReceivePort_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtAlpcSendWaitReceivePort"));
    if (!NtAlpcConnectPort || !NtAlpcSendWaitReceivePort) return false;

    wchar_t portNameBuf[64];
    alpc_port_name(portNameBuf, 64);
    UNICODE_STRING portName;
    portName.Buffer = portNameBuf;
    portName.Length = (USHORT)(wcslen(portNameBuf) * sizeof(wchar_t));
    portName.MaximumLength = portName.Length + sizeof(wchar_t);

    HANDLE hPort = nullptr;
    NTSTATUS st = NtAlpcConnectPort(&hPort, &portName, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    if (!NT_SUCCESS(st) || !hPort) return false;

    /* Handshake: write REQ_INIT to section, send minimal message, wait reply */
    BYTE* buf = (BYTE*)g_shared_buf;
    ZeroMemory(buf, FLUSHCOMM_SECTION_SIZE);
    *(ULONG64*)buf = FLUSHCOMM_MAGIC;
    *(ULONG*)(buf + 8) = (ULONG)REQ_INIT;
    *(NTSTATUS*)(buf + 80) = (NTSTATUS)0xDEADBEEF;

    ALPC_MSG sendMsg = {}, recvMsg = {};
    sendMsg.datalen = 1;
    sendMsg.data[0] = (UCHAR)REQ_INIT;
    ULONG recvLen = sizeof(recvMsg);
    LARGE_INTEGER timeout;
    timeout.QuadPart = -10000000LL; /* 1 second */
    st = NtAlpcSendWaitReceivePort(hPort, 0, &sendMsg, NULL, &recvMsg, &recvLen, NULL, &timeout);
    if (!NT_SUCCESS(st)) {
        CloseHandle(hPort);
        return false;
    }
    if (*(NTSTATUS*)(buf + 80) != STATUS_SUCCESS) {
        CloseHandle(hPort);
        return false;
    }
    g_alpc_port = hPort;
    g_use_alpc = true;
    return true;
}

/* Send request via ALPC (section already filled by caller) */
static bool send_request_alpc(REQUEST_TYPE type, PVOID args) {
    if (!g_alpc_port || !g_shared_buf) return false;
    BYTE* buf = (BYTE*)g_shared_buf;
    SIZE_T bufSize = FLUSHCOMM_SECTION_SIZE;
    ZeroMemory(buf, bufSize);
    *(ULONG64*)buf = FLUSHCOMM_MAGIC;
    *(ULONG*)(buf + 8) = (ULONG)type;
    if (args) memcpy(buf + 16, args, 64);

    typedef NTSTATUS(NTAPI* NtAlpcSendWaitReceivePort_t)(HANDLE, ULONG, PVOID, PVOID, PVOID, PULONG, PVOID, PLARGE_INTEGER);
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    auto NtAlpcSendWaitReceivePort = (NtAlpcSendWaitReceivePort_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtAlpcSendWaitReceivePort"));
    if (!NtAlpcSendWaitReceivePort) return false;

    ALPC_MSG sendMsg = {}, recvMsg = {};
    sendMsg.datalen = 1;
    sendMsg.data[0] = (UCHAR)(type & 0xFF);
    ULONG recvLen = sizeof(recvMsg);
    LARGE_INTEGER timeout;
    timeout.QuadPart = -50000000LL; /* 5 second */
    NTSTATUS st = NtAlpcSendWaitReceivePort(g_alpc_port, 0, &sendMsg, NULL, &recvMsg, &recvLen, NULL, &timeout);
    return NT_SUCCESS(st);
}
