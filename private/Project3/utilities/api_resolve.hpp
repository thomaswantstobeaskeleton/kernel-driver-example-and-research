#pragma once
/* Runtime-decrypted strings for GetProcAddress/GetModuleHandle.
 * PEB-based resolution preferred - avoids GetModuleHandleW/GetProcAddress (commonly hooked by EAC).
 * Parse PEB->Ldr->InMemoryOrderModuleList + PE export table; fallback to kernel32 APIs.
 * Key from FLUSHCOMM_OBF_BASE when available - no single public literal as project-wide signature. */

#include <Windows.h>
#include <winternl.h>
/* Key from config when available. Projects that have flush_comm_config.h in include path get FLUSHCOMM_OBF_BASE from it; others (e.g. Frozen Public) use default so this header compiles without that file. */
#ifdef __has_include
#  if __has_include("flush_comm_config.h")
#    include "flush_comm_config.h"
#  endif
#endif
/* When config not present: derive from build time so no single public literal (0x9D/0x5A) as project-wide signature. */
#ifndef FLUSHCOMM_OBF_BASE
#  define FLUSHCOMM_OBF_BASE ((unsigned char)(0x80 + (((__TIME__[7]-'0')*10+(__TIME__[6]-'0')) % 64)))
#endif

namespace api_resolve {

#define APIRES_OBF_KEY  ((unsigned char)(FLUSHCOMM_OBF_BASE))

/* Build encrypted string at compile time. Usage: APIRES_OBF_A("NtGetNextProcess") */
template<size_t N>
struct ObfBuf {
    char data[N];
    constexpr ObfBuf(const char (&s)[N]) : data{} {
        for (size_t i = 0; i < N; i++)
            data[i] = s[i] ^ (APIRES_OBF_KEY + (char)(i & 0xFF));
    }
};
template<size_t N>
struct ObfBufW {
    wchar_t data[N];
    constexpr ObfBufW(const wchar_t (&s)[N]) : data{} {
        for (size_t i = 0; i < N; i++)
            data[i] = (wchar_t)(s[i] ^ (APIRES_OBF_KEY + (char)(i & 0xFF)));
    }
};

#define APIRES_OBF_A(s) (::api_resolve::ObfBuf<sizeof(s)>(s))
#define APIRES_OBF_W(s) (::api_resolve::ObfBufW<sizeof(s)/sizeof(wchar_t)>(s))

inline void decrypt(char* out, const char* enc, size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = enc[i] ^ (APIRES_OBF_KEY + (char)(i & 0xFF));
}
inline void decrypt_w(wchar_t* out, const wchar_t* enc, size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = (wchar_t)(enc[i] ^ (APIRES_OBF_KEY + (char)(i & 0xFF)));
}

/* PEB-based: walk Ldr->InMemoryOrderModuleList, find module by base name. No GetModuleHandle. */
inline HMODULE peb_get_module(const wchar_t* name) {
#ifdef _WIN64
    PPEB peb = (PPEB)__readgsqword(0x60);
#else
    PPEB peb = (PPEB)__readfsdword(0x30);
#endif
    if (!peb || !peb->Ldr) return nullptr;
    PLIST_ENTRY head = (PLIST_ENTRY)((BYTE*)peb->Ldr + 0x20); /* InMemoryOrderModuleList */
    PLIST_ENTRY cur = head->Flink;
    while (cur != head) {
        /* LDR_DATA_TABLE_ENTRY: InMemoryOrderLinks at +0x10, DllBase +0x30, FullDllName +0x48 */
        BYTE* entry = (BYTE*)cur - 0x10;
        PVOID base = *(PVOID*)(entry + 0x30);
        PUNICODE_STRING full = (PUNICODE_STRING)(entry + 0x48);
        if (base && full->Buffer && full->Length >= 2) {
            USHORT len = full->Length / sizeof(WCHAR);
            const WCHAR* p = full->Buffer;
            const WCHAR* last = p + len - 1;
            while (last > p && *last != L'\\' && *last != L'/') --last;
            if (*last == L'\\' || *last == L'/') ++last;
            /* Compare base name case-insensitive */
            const WCHAR* q = name;
            while (*q && last < full->Buffer + len) {
                WCHAR c1 = *last, c2 = *q;
                if (c1 >= L'A' && c1 <= L'Z') c1 += 32;
                if (c2 >= L'A' && c2 <= L'Z') c2 += 32;
                if (c1 != c2) break;
                ++last; ++q;
            }
            if (!*q && (last >= full->Buffer + len || *last == L'\0'))
                return (HMODULE)base;
        }
        cur = cur->Flink;
    }
    return nullptr;
}

/* PEB-based: parse PE export table for function. No GetProcAddress. */
inline FARPROC peb_get_proc(HMODULE mod, const char* name) {
    if (!mod || !name) return nullptr;
    BYTE* base = (BYTE*)mod;
    __try {
        if (*(WORD*)base != IMAGE_DOS_SIGNATURE) return nullptr;
        DWORD off = *(DWORD*)(base + 0x3C);
        if (off > 0x1000) return nullptr;
        if (*(DWORD*)(base + off) != IMAGE_NT_SIGNATURE) return nullptr;
        DWORD exportRva = *(DWORD*)(base + off + 0x88); /* OptionalHeader.DataDirectory[0] */
        if (!exportRva) return nullptr;
        BYTE* exp = base + exportRva;
        DWORD numNames = *(DWORD*)(exp + 0x18);
        DWORD addrNames = *(DWORD*)(exp + 0x20);
        DWORD addrOrds = *(DWORD*)(exp + 0x24);
        DWORD addrFuncs = *(DWORD*)(exp + 0x1C);
        for (DWORD i = 0; i < numNames; i++) {
            DWORD nameRva = *(DWORD*)(base + addrNames + i * 4);
            const char* expName = (const char*)(base + nameRva);
            const char* n = name;
            while (*n && *expName && *n == *expName) { ++n; ++expName; }
            if (*n == *expName) {
                WORD ord = *(WORD*)(base + addrOrds + i * 2);
                DWORD funcRva = *(DWORD*)(base + addrFuncs + ord * 4);
                return (FARPROC)(base + funcRva);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { (void)0; }
    return nullptr;
}

/* Get module: PEB first, then fallback */
inline HMODULE get_module(const wchar_t* enc, size_t n) {
    wchar_t buf[64];
    if (n >= 64) return nullptr;
    decrypt_w(buf, enc, n);
    buf[n] = 0;
    HMODULE h = peb_get_module(buf);
    return h ? h : GetModuleHandleW(buf);
}

/* Get proc: PEB export parse first, then fallback */
inline FARPROC get_proc(HMODULE mod, const char* enc, size_t n) {
    char buf[64];
    if (n >= 64) return nullptr;
    decrypt(buf, enc, n);
    buf[n] = 0;
    FARPROC p = peb_get_proc(mod, buf);
    return p ? p : GetProcAddress(mod, buf);
}

/* Helpers - pass APIRES_OBF_A("str") or APIRES_OBF_W(L"str"). N includes null terminator. */
template<size_t N>
inline HMODULE get_module_w(const ObfBufW<N>& obf) {
    wchar_t buf[64];
    if (N > 64) return nullptr;
    decrypt_w(buf, obf.data, N - 1);
    buf[N - 1] = 0;
    HMODULE h = peb_get_module(buf);
    return h ? h : GetModuleHandleW(buf);
}
template<size_t N>
inline FARPROC get_proc_a(HMODULE mod, const ObfBuf<N>& obf) {
    char buf[64];
    if (N > 64) return nullptr;
    decrypt(buf, obf.data, N - 1);
    buf[N - 1] = 0;
    FARPROC p = peb_get_proc(mod, buf);
    return p ? p : GetProcAddress(mod, buf);
}

} // namespace api_resolve
