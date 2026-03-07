#pragma once
/* Custom file SHA256 - NtOpenFile/NtReadFile (no CreateFile, no CryptoAPI).
 * Inline SHA256 implementation - no crypt32/advapi32, no BCrypt.
 * Win11 23H2 compatible. Path: \\??\\C:\\path format for NtOpenFile. */

#include <Windows.h>
#include <winternl.h>
#include <string>
#include "api_resolve.hpp"

typedef long NTSTATUS;
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)

#ifndef FILE_READ_DATA
#define FILE_READ_DATA 0x0001
#endif
#ifndef FILE_SHARE_READ
#define FILE_SHARE_READ 0x00000001
#endif
#ifndef FILE_NON_DIRECTORY_FILE
#define FILE_NON_DIRECTORY_FILE 0x00000040
#endif
#ifndef OBJ_CASE_INSENSITIVE
#define OBJ_CASE_INSENSITIVE 0x00000040
#endif

/* Minimal SHA256 - no external crypto lib. Round constants derived at compile time. */
namespace _sha256_impl {
    static inline void process_block(const unsigned char* block, unsigned int* state) {
        static const unsigned int k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };
        unsigned int w[64], a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 16; i++)
            w[i] = (unsigned int)block[i*4]<<24 | (unsigned int)block[i*4+1]<<16 | (unsigned int)block[i*4+2]<<8 | (unsigned int)block[i*4+3];
        for (int i = 16; i < 64; i++) {
            unsigned int s0 = (w[i-15]>>7|w[i-15]<<25)^(w[i-15]>>18|w[i-15]<<14)^(w[i-15]>>3);
            unsigned int s1 = (w[i-2]>>17|w[i-2]<<15)^(w[i-2]>>19|w[i-2]<<13)^(w[i-2]>>10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        for (int i = 0; i < 64; i++) {
            unsigned int S1 = (e>>6|e<<26)^(e>>11|e<<21)^(e>>25|e<<7);
            unsigned int ch = (e&f)^(~e&g);
            unsigned int t1 = h + S1 + ch + k[i] + w[i];
            unsigned int S0 = (a>>2|a<<30)^(a>>13|a<<19)^(a>>22|a<<10);
            unsigned int maj = (a&b)^(a&c)^(b&c);
            unsigned int t2 = S0 + maj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
}

inline std::string file_sha256_nt(const std::string& filePath) {
    typedef NTSTATUS(NTAPI* NtOpenFile_t)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ULONG, ULONG);
    typedef NTSTATUS(NTAPI* NtReadFile_t)(HANDLE, HANDLE, void*, void*, PIO_STATUS_BLOCK, PVOID, ULONG, PLARGE_INTEGER, PULONG);
    typedef NTSTATUS(NTAPI* NtClose_t)(HANDLE);
    static NtOpenFile_t pNtOpenFile = nullptr;
    static NtReadFile_t pNtReadFile = nullptr;
    static NtClose_t pNtClose = nullptr;
    if (!pNtOpenFile) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        if (!ntdll) return "";
        pNtOpenFile = (NtOpenFile_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtOpenFile"));
        pNtReadFile = (NtReadFile_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtReadFile"));
        pNtClose = (NtClose_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtClose"));
        if (!pNtOpenFile || !pNtReadFile || !pNtClose) return "";
    }
    std::wstring pathW;
    pathW.resize(filePath.size() + 1);
    int n = MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), (int)filePath.size(), &pathW[0], (int)pathW.size());
    if (n <= 0) return "";
    pathW.resize(n);
    if (pathW.size() >= 2 && pathW[1] == L':') {
        pathW = L"\\??\\" + pathW;
    }
    UNICODE_STRING ustr;
    ustr.Length = (USHORT)(pathW.size() * sizeof(wchar_t));
    ustr.MaximumLength = ustr.Length + sizeof(wchar_t);
    ustr.Buffer = &pathW[0];
    OBJECT_ATTRIBUTES oa = { sizeof(oa), NULL, &ustr, OBJ_CASE_INSENSITIVE };
    IO_STATUS_BLOCK iosb = { 0 };
    HANDLE hFile = NULL;
    if (!NT_SUCCESS(pNtOpenFile(&hFile, FILE_READ_DATA | 0x00100000, &oa, &iosb, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE)))
        return "";
    unsigned int state[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    unsigned char block[64];
    ULONGLONG total = 0;
    for (;;) {
        iosb.Status = 0; iosb.Information = 0;
        if (!NT_SUCCESS(pNtReadFile(hFile, NULL, NULL, NULL, &iosb, block, 64, NULL, NULL)))
            break;
        if (iosb.Information == 0) break;
        total += iosb.Information;
        if (iosb.Information < 64) {
            ULONG len = (ULONG)iosb.Information;
            block[len] = 0x80;
            for (ULONG i = len + 1; i < 64; i++) block[i] = 0;
            if (len >= 56) {
                _sha256_impl::process_block(block, state);
                for (int i = 0; i < 64; i++) block[i] = 0;
            }
            for (int i = 56; i < 64; i++)
                block[i] = (unsigned char)((total * 8ULL) >> ((63-i)*8));
            _sha256_impl::process_block(block, state);
            break;
        }
        _sha256_impl::process_block(block, state);
    }
    pNtClose(hFile);
    char hex[65];
    for (int i = 0; i < 8; i++)
        sprintf_s(hex + i*8, 9, "%08X", state[i]);
    hex[64] = 0;
    return std::string(hex);
}
