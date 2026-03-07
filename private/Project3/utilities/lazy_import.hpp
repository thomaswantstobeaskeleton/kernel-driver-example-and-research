#pragma once
/* Optional lazy/dynamic import - resolve APIs at runtime via GetProcAddress.
 * Reduces IAT visibility for CreateFileW, DeviceIoControl, etc.
 * Include and use LazyImport::CreateFileW, etc. instead of direct calls.
 * Enable with FLUSHCOMM_USE_LAZY_IMPORT 1 in flush_comm_config.h */

#ifdef _WIN32
#include <Windows.h>
#include <winreg.h>

#include "../flush_comm_config.h"
#include "api_resolve.hpp"

#ifndef FLUSHCOMM_USE_LAZY_IMPORT
#define FLUSHCOMM_USE_LAZY_IMPORT 0
#endif

#if FLUSHCOMM_USE_LAZY_IMPORT

namespace LazyImport {

using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using DeviceIoControl_t = BOOL(WINAPI*)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
using RegCreateKeyExW_t = LSTATUS(WINAPI*)(HKEY, LPCWSTR, DWORD, LPSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
using RegSetValueExW_t = LSTATUS(WINAPI*)(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
using RegQueryValueExW_t = LSTATUS(WINAPI*)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
using RegOpenKeyExW_t = LSTATUS(WINAPI*)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
using RegCloseKey_t = LSTATUS(WINAPI*)(HKEY);
using OpenFileMappingW_t = HANDLE(WINAPI*)(DWORD, BOOL, LPCWSTR);
using CreateFileMappingW_t = HANDLE(WINAPI*)(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD, LPCWSTR);
using GetWindowsDirectoryW_t = UINT(WINAPI*)(LPWSTR, UINT);
using MapViewOfFile_t = LPVOID(WINAPI*)(HANDLE, DWORD, DWORD, DWORD, SIZE_T);
using UnmapViewOfFile_t = BOOL(WINAPI*)(LPCVOID);
using FlushFileBuffers_t = BOOL(WINAPI*)(HANDLE);
using CloseHandle_t = BOOL(WINAPI*)(HANDLE);

inline CreateFileW_t CreateFileW = nullptr;
inline DeviceIoControl_t DeviceIoControl = nullptr;
inline RegCreateKeyExW_t RegCreateKeyExW = nullptr;
inline RegSetValueExW_t RegSetValueExW = nullptr;
inline RegQueryValueExW_t RegQueryValueExW = nullptr;
inline RegOpenKeyExW_t RegOpenKeyExW = nullptr;
inline RegCloseKey_t RegCloseKey = nullptr;
inline OpenFileMappingW_t OpenFileMappingW = nullptr;
inline CreateFileMappingW_t CreateFileMappingW = nullptr;
inline GetWindowsDirectoryW_t GetWindowsDirectoryW = nullptr;
inline MapViewOfFile_t MapViewOfFile = nullptr;
inline UnmapViewOfFile_t UnmapViewOfFile = nullptr;
inline FlushFileBuffers_t FlushFileBuffers = nullptr;
inline CloseHandle_t CloseHandle = nullptr;

inline bool Init() {
    HMODULE k32 = api_resolve::get_module_w(APIRES_OBF_W(L"kernel32.dll"));
    HMODULE adv = api_resolve::get_module_w(APIRES_OBF_W(L"advapi32.dll"));
    if (!k32 || !adv) return false;
    CreateFileW = (CreateFileW_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("CreateFileW"));
#if !FLUSHCOMM_USE_FLUSH_BUFFERS
    DeviceIoControl = (DeviceIoControl_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("DeviceIoControl"));
#endif
    RegCreateKeyExW = (RegCreateKeyExW_t)api_resolve::get_proc_a(adv, APIRES_OBF_A("RegCreateKeyExW"));
    RegSetValueExW = (RegSetValueExW_t)api_resolve::get_proc_a(adv, APIRES_OBF_A("RegSetValueExW"));
    RegQueryValueExW = (RegQueryValueExW_t)api_resolve::get_proc_a(adv, APIRES_OBF_A("RegQueryValueExW"));
    RegOpenKeyExW = (RegOpenKeyExW_t)api_resolve::get_proc_a(adv, APIRES_OBF_A("RegOpenKeyExW"));
    RegCloseKey = (RegCloseKey_t)api_resolve::get_proc_a(adv, APIRES_OBF_A("RegCloseKey"));
    OpenFileMappingW = (OpenFileMappingW_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("OpenFileMappingW"));
    CreateFileMappingW = (CreateFileMappingW_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("CreateFileMappingW"));
    GetWindowsDirectoryW = (GetWindowsDirectoryW_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("GetWindowsDirectoryW"));
    MapViewOfFile = (MapViewOfFile_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("MapViewOfFile"));
    UnmapViewOfFile = (UnmapViewOfFile_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("UnmapViewOfFile"));
    FlushFileBuffers = (FlushFileBuffers_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("FlushFileBuffers"));
    CloseHandle = (CloseHandle_t)api_resolve::get_proc_a(k32, APIRES_OBF_A("CloseHandle"));
    return CreateFileW && RegCreateKeyExW && RegSetValueExW &&
           RegQueryValueExW && RegOpenKeyExW && RegCloseKey && OpenFileMappingW && CreateFileMappingW && GetWindowsDirectoryW &&
           MapViewOfFile && UnmapViewOfFile && FlushFileBuffers && CloseHandle
#if !FLUSHCOMM_USE_FLUSH_BUFFERS
           && DeviceIoControl
#endif
           ;
}

} // namespace LazyImport

#endif /* FLUSHCOMM_USE_LAZY_IMPORT */

#endif /* _WIN32 */
