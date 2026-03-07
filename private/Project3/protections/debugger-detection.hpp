#include <synchapi.h>
#include <thread>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <dbghelp.h>
#include <iphlpapi.h>
#include <intrin.h>
#include <cstring> 
#include <cstdlib> 
#include <wintrust.h>
#include <softpub.h>
#include <algorithm>
#include "../utilities/peb_modules.hpp"
#include "../utilities/direct_syscall.hpp"
#include "../utilities/api_resolve.hpp"
#include "../utilities/str_obfuscate.hpp"
#include "../utilities/file_hash_nt.hpp"
#pragma comment (lib, "wintrust")
#pragma comment (lib, "psapi")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "Dbghelp.lib")







/* PEB-based - avoids EnumProcessModules/GetModuleInformation (monitored) */
bool IsAddressInPEBModules(void* address) {
    return peb_modules::is_address_in_loaded_modules(address);
}

bool DetectManualMappedDll() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    BYTE* addr = nullptr;
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T retLen = 0;

    /* NtQueryVirtualMemory - bypasses VirtualQuery (kernel32 path, often hooked) */
    while (addr < (BYTE*)sysInfo.lpMaximumApplicationAddress) {
        if (!NT_SUCCESS(sys_NtQueryVirtualMemory(GetCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &retLen))) {
            addr += 0x1000;
            continue;
        }

        if ((mbi.State == MEM_COMMIT) &&
            (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
            (mbi.Type == MEM_PRIVATE)) {

            __try {
                auto* dos = (IMAGE_DOS_HEADER*)mbi.BaseAddress;
                if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                    goto skip;

                auto* nt = (IMAGE_NT_HEADERS*)((BYTE*)dos + dos->e_lfanew);
                if (nt->Signature != IMAGE_NT_SIGNATURE)
                    goto skip;

                if (!IsAddressInPEBModules(mbi.BaseAddress)) {
                    return true;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
            }

        }

    skip:
        addr += mbi.RegionSize;
    }

    return false;
}

namespace vmDetection {

    using UINT64 = unsigned long long;

    inline UINT64 rdtsc() {
        return __rdtsc();
    }

    bool hypervisor_present_flag() {
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        return (cpuInfo[2] & (1 << 31)) != 0;
    }

    bool cpu_known_vm_vendors() {
        if (!hypervisor_present_flag()) return false;
        int cpuInfo[4];
        __cpuid(cpuInfo, 0x40000000);
        char hyperVendorId[13] = { 0 };
        memcpy(&hyperVendorId[0], &cpuInfo[1], 4);
        memcpy(&hyperVendorId[4], &cpuInfo[2], 4);
        memcpy(&hyperVendorId[8], &cpuInfo[3], 4);
        hyperVendorId[12] = '\0';
        /* Vendor strings obfuscated - no literal "VMware"/"VBox" in .rdata */
        if (strncmp(OBF_STR("KVMKVMKVM").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR("Microsoft Hv").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR("VMwareVMware").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR("XenVMMXenVMM").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR("prl hyperv  ").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR("VBoxVBoxVBox").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR("bhyve bhyve").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR("TCGTCGTCGTCG").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR(" lrpepyh  vr").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR("ACRNACRNACRN").c_str(), hyperVendorId, 12) == 0) return true;
        if (strncmp(OBF_STR(" QNXQVMBSQG ").c_str(), hyperVendorId, 12) == 0) return true;
        return false;
    }

    /* MAC prefixes encoded. GetAdaptersInfo - no winsock dep; works on Win11 23H2. Resolved via api_resolve. */
    static constexpr BYTE _MK = 0xA7;
    static const BYTE _enc1[] = { 0xA7, 0xA2, 0xCE };  /* 00,05,69 ^ _MK */
    static const BYTE _enc2[] = { 0xA7, 0xAB, 0x8E };  /* 00,0C,29 */
    static const BYTE _enc3[] = { 0xA7, 0xF7, 0xF1 };  /* 00,50,56 */
    static const BYTE _enc4[] = { 0xAF, 0xA7, 0x80 };  /* 08,00,27 */
    bool vm_mac_prefix_detected() {
        typedef ULONG(WINAPI* GetAdaptersInfo_t)(PVOID, PULONG);
        static GetAdaptersInfo_t pGAI = nullptr;
        if (!pGAI) {
            HMODULE iphlp = api_resolve::get_module_w(APIRES_OBF_W(L"iphlpapi.dll"));
            if (!iphlp) return false;
            pGAI = (GetAdaptersInfo_t)api_resolve::get_proc_a(iphlp, APIRES_OBF_A("GetAdaptersInfo"));
            if (!pGAI) return false;
        }
        ULONG buflen = 0;
        if (pGAI(NULL, &buflen) != ERROR_BUFFER_OVERFLOW || buflen < sizeof(IP_ADAPTER_INFO)) return false;
        std::vector<BYTE> buf(buflen);
        PIP_ADAPTER_INFO pAdapters = (PIP_ADAPTER_INFO)buf.data();
        if (pGAI(pAdapters, &buflen) != ERROR_SUCCESS) return false;
        for (PIP_ADAPTER_INFO p = pAdapters; p; p = p->Next) {
            if (p->AddressLength < 3) continue;
            BYTE* m = p->Address;
            if ((m[0]==(_enc1[0]^_MK) && m[1]==(_enc1[1]^_MK) && m[2]==(_enc1[2]^_MK)) ||
                (m[0]==(_enc2[0]^_MK) && m[1]==(_enc2[1]^_MK) && m[2]==(_enc2[2]^_MK)) ||
                (m[0]==(_enc3[0]^_MK) && m[1]==(_enc3[1]^_MK) && m[2]==(_enc3[2]^_MK)) ||
                (m[0]==(_enc4[0]^_MK) && m[1]==(_enc4[1]^_MK) && m[2]==(_enc4[2]^_MK)))
                return true;
        }
        return false;
    }

    /* Registry-based VM detection - no WMI (CoCreateInstance/IWbemServices heavily monitored by EAC).
     * Reads HKLM\HARDWARE\DESCRIPTION\System\BIOS directly. Same data as Win32_BIOS/Win32_BaseBoard. */
    static bool reg_query_str(HKEY hKey, const wchar_t* valName, std::wstring& out) {
        wchar_t buf[256] = { 0 };
        DWORD sz = sizeof(buf);
        if (RegQueryValueExW(hKey, valName, NULL, NULL, (LPBYTE)buf, &sz) != ERROR_SUCCESS) return false;
        out = buf;
        return !out.empty();
    }
    static bool reg_str_contains_vm(const std::wstring& data) {
        if (data.empty()) return false;
        if (data.find(OBF_WSTR(L"VMware")) != std::wstring::npos) return true;
        if (data.find(OBF_WSTR(L"VirtualBox")) != std::wstring::npos) return true;
        if (data.find(OBF_WSTR(L"VBOX")) != std::wstring::npos) return true;
        if (data.find(OBF_WSTR(L"Xen")) != std::wstring::npos) return true;
        if (data.find(OBF_WSTR(L"QEMU")) != std::wstring::npos) return true;
        if (data.find(OBF_WSTR(L"Microsoft Corporation")) != std::wstring::npos) return true;
        if (data.find(OBF_WSTR(L"Hyper-V")) != std::wstring::npos) return true;
        return false;
    }
    bool bios_or_baseboard_vm_string_detected() {
        HKEY hKey = NULL;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, OBF_WSTR(L"HARDWARE\\DESCRIPTION\\System\\BIOS").c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            return false;
        std::wstring s;
        bool found = false;
        if (reg_query_str(hKey, OBF_WSTR(L"SystemManufacturer").c_str(), s) && reg_str_contains_vm(s)) found = true;
        if (!found && reg_query_str(hKey, OBF_WSTR(L"BaseBoardManufacturer").c_str(), s) && reg_str_contains_vm(s)) found = true;
        if (!found && reg_query_str(hKey, OBF_WSTR(L"BaseBoardProduct").c_str(), s) && reg_str_contains_vm(s)) found = true;
        if (!found && reg_query_str(hKey, OBF_WSTR(L"SystemProductName").c_str(), s) && reg_str_contains_vm(s)) found = true;
        if (!found && reg_query_str(hKey, OBF_WSTR(L"BIOSVendor").c_str(), s) && reg_str_contains_vm(s)) found = true;
        RegCloseKey(hKey);
        return found;
    }

    bool isVM() {
        int score = 0;

        if (!hypervisor_present_flag()) return false;

        if (cpu_known_vm_vendors()) score++;
        if (vm_mac_prefix_detected()) score++;
        if (bios_or_baseboard_vm_string_detected()) score++;

        /* Silent exit - no MessageBox (monitored), no system() (spawns cmd). Never taskkill svchost. */
        if (score >= 2) {
            TerminateProcess(GetCurrentProcess(), 0);
            std::exit(0);
        }

        return false;
    }

} 

std::string GetSystemDir() {
    char sysDir[MAX_PATH];
    GetSystemDirectoryA(sysDir, MAX_PATH);
    return std::string(sysDir);
}

std::string GetSelfModuleName() {
    std::string s = peb_modules::get_self_module_name();
    return s.empty() ? "unknown" : s;  /* PEB-based; fallback only if PEB walk fails */
}

bool isTrustedDllPath(const std::string& path) {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find(OBF_STR("c:\\program files\\windows defender")) != std::string::npos) return true;
    if (lower.find(OBF_STR("c:\\program files\\avast software")) != std::string::npos) return true;
    if (lower.find(OBF_STR("c:\\program files\\avg")) != std::string::npos) return true;
    if (lower.find(OBF_STR("c:\\program files\\bitdefender")) != std::string::npos) return true;
    if (lower.find(OBF_STR("c:\\program files\\kaspersky lab")) != std::string::npos) return true;
    if (lower.find(OBF_STR("c:\\program files\\eset")) != std::string::npos) return true;
    if (lower.find(OBF_STR("c:\\program files\\mcafee")) != std::string::npos) return true;
    if (lower.find(OBF_STR("c:\\program files\\norton")) != std::string::npos) return true;
    if (lower.find(OBF_STR("c:\\program files\\malwarebytes")) != std::string::npos) return true;
    return false;
}

bool IsFileDigitallySigned(const std::string& filePath) {
    WINTRUST_FILE_INFO fileInfo = { 0 };
    std::wstring widePath(filePath.begin(), filePath.end());
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = widePath.c_str();
    fileInfo.hFile = NULL;
    fileInfo.pgKnownSubject = NULL;

    WINTRUST_DATA winTrustData = { 0 };
    winTrustData.cbStruct = sizeof(WINTRUST_DATA);
    winTrustData.dwUIChoice = WTD_UI_NONE;
    winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    winTrustData.pFile = &fileInfo;
    winTrustData.dwStateAction = 0;
    winTrustData.hWVTStateData = NULL;
    winTrustData.dwProvFlags = WTD_SAFER_FLAG;
    winTrustData.pwszURLReference = NULL;

    GUID actionGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(NULL, &actionGUID, &winTrustData);
    return (status == ERROR_SUCCESS);
}

/* PEB-based - avoids EnumProcessModules/GetModuleFileNameEx (monitored) */
bool detectInjectedDlls() {
    peb_modules::ModuleInfo pebMods[512];
    int count = peb_modules::enumerate_modules(pebMods, 512);

    std::string selfExe = GetSelfModuleName();
    char winDir[MAX_PATH];
    GetWindowsDirectoryA(winDir, MAX_PATH);
    std::string windowsDir = winDir;
    std::string systemDir = GetSystemDir();
    std::transform(systemDir.begin(), systemDir.end(), systemDir.begin(), ::tolower);
    std::transform(windowsDir.begin(), windowsDir.end(), windowsDir.begin(), ::tolower);

    for (int i = 0; i < count; i++) {
        char moduleName[MAX_PATH] = { 0 };
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, moduleName, MAX_PATH, pebMods[i].name, MAX_PATH);
        std::string fullPath = moduleName;
        std::string modName = fullPath.substr(fullPath.find_last_of("\\/") + 1);

        if (_stricmp(modName.c_str(), selfExe.c_str()) == 0)
            continue;

        std::string lowerPath = fullPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

        if (lowerPath.find(systemDir) != std::string::npos ||
            lowerPath.find(windowsDir) != std::string::npos ||
            isTrustedDllPath(lowerPath) ||
            IsFileDigitallySigned(fullPath)) {
            continue;
        }

        /* No system("cls") - spawns cmd. Silent detection. */
        return true;
    }
    return false;
}


std::string ToUpper(const std::string& str);
std::string CalcSHA256(const std::string& filePath);

std::string ToUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

/* Check if any process has exe matching targetHash (uppercase SHA256). Uses NtGetNextProcess, not EnumProcesses/OpenProcess. */
static bool process_hash_exists_nt(const std::string& targetHash) {
    struct Ctx { bool found; std::string target; };
    Ctx ctx = { false, ToUpper(targetHash) };
    enumerate_processes_nt([](DWORD, const char* path, void* v) {
        Ctx* c = (Ctx*)v;
        if (c->found) return;
        std::string hash = CalcSHA256(path);
        if (!hash.empty() && ToUpper(hash) == c->target) c->found = true;
    }, &ctx);
    return ctx.found;
}

/* NtOpenFile + NtReadFile + custom SHA256 (file_hash_nt.hpp) - no CreateFile, no CryptoAPI */
inline std::string CalcSHA256(const std::string& filePath) {
    return file_sha256_nt(filePath);
}

/* Tool hashes obfuscated - no literal SHA256 hex in .rdata */
bool FileGrabHashCheck() { return process_hash_exists_nt(OBF_STR("11B8CDD387370DE1D162516B82376ECF28D321DC8F46EBCCE389DCCC2A5A4CC9")); }
bool processhackerHashCheck() { return process_hash_exists_nt(OBF_STR("2948E04DB7712D78A7BF31C497BDF6278479B6BC8D9402AE8D7FBD148F3A0FDD")); }
bool Xenosinjector64HashCheck() { return process_hash_exists_nt(OBF_STR("922163713B973CF8C4AD80F16CF69305ED0CA319B314E7EC8AE1982ED5F2A9ED")); }
bool ExtremeinjectorHashCheck() { return process_hash_exists_nt(OBF_STR("B65F40618F584303CA0BCF9B5F88C233CC4237699C0C4BF40BA8FACBE8195A46")); }
bool XenosinjectorHashCheck() { return process_hash_exists_nt(OBF_STR("DEF1C2F12307D598E42506A55F1A06ED5E652AF0D260AAC9572469429F10D04D")); }
bool processhacker2HashCheck() { return process_hash_exists_nt(OBF_STR("BD2C2CF0631D881ED382817AFCCE2B093F4E412FFB170A719E2762F250ABFEA4")); }
bool everthinghashcheck() { return process_hash_exists_nt(OBF_STR("1E9394A3144167AD33DDEC3137D4F34DCF1AF94442F2FF36873DE58AD465540D")); }

/* Exit without spawning cmd/taskkill - NtTerminateProcess is stealthier than system() */
static void secure_exit() {
    TerminateProcess(GetCurrentProcess(), 0);
    std::exit(0);
}

/* NtQueryInformationProcess (undocumented class 7=DebugPort, 30=DebugObject) - no system()/tasklist */
static bool nt_debugger_attached() {
    typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static NtQueryInformationProcess_t NtQIP = nullptr;
    if (!NtQIP) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        NtQIP = (NtQueryInformationProcess_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQueryInformationProcess"));
        if (!NtQIP) return false;
    }
    HANDLE hSelf = GetCurrentProcess();
    ULONG_PTR debugPort = 0;
    ULONG retLen = 0;
    if (NT_SUCCESS(NtQIP(hSelf, 7, &debugPort, sizeof(debugPort), &retLen)) && debugPort != 0)
        return true;
    HANDLE debugObj = nullptr;
    if (NT_SUCCESS(NtQIP(hSelf, 30, &debugObj, sizeof(debugObj), &retLen)) && debugObj != nullptr)
        return true;
    return false;
}

/* EnumWindows + GetWindowText - avoids FindWindowA (common debugger check, often hooked).
 * Resolves user32 APIs via api_resolve. Case-insensitive substring match. */
struct _DbgWndCtx { int(WINAPI* getText)(HWND, LPWSTR, int); bool* found; };
static BOOL CALLBACK _enum_dbg_wnd_proc(HWND hwnd, LPARAM lp) {
    _DbgWndCtx* ctx = (_DbgWndCtx*)lp;
    if (!ctx || !ctx->getText || !ctx->found) return TRUE;
    wchar_t title[260] = { 0 };
    if (ctx->getText(hwnd, title, 259) <= 0) return TRUE;
    std::wstring t(title);
    std::transform(t.begin(), t.end(), t.begin(), ::towlower);
    if (t.find(OBF_WSTR(L"x64dbg")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    if (t.find(OBF_WSTR(L"x32dbg")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    if (t.find(OBF_WSTR(L"ida")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    if (t.find(OBF_WSTR(L"ollydbg")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    if (t.find(OBF_WSTR(L"windbg")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    if (t.find(OBF_WSTR(L"process hacker")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    if (t.find(OBF_WSTR(L"cheat engine")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    if (t.find(OBF_WSTR(L"ghidra")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    if (t.find(OBF_WSTR(L"hxd")) != std::wstring::npos) { *ctx->found = true; return FALSE; }
    return TRUE;
}
static bool debugger_window_present() {
    typedef BOOL(WINAPI* EnumWindows_t)(WNDENUMPROC, LPARAM);
    typedef int(WINAPI* GetWindowTextW_t)(HWND, LPWSTR, int);
    static EnumWindows_t pEnumWindows = nullptr;
    static GetWindowTextW_t pGetWindowTextW = nullptr;
    if (!pEnumWindows) {
        HMODULE user32 = api_resolve::get_module_w(APIRES_OBF_W(L"user32.dll"));
        if (!user32) return false;
        pEnumWindows = (EnumWindows_t)api_resolve::get_proc_a(user32, APIRES_OBF_A("EnumWindows"));
        pGetWindowTextW = (GetWindowTextW_t)api_resolve::get_proc_a(user32, APIRES_OBF_A("GetWindowTextW"));
        if (!pEnumWindows || !pGetWindowTextW) return false;
    }
    bool found = false;
    _DbgWndCtx ctx = { pGetWindowTextW, &found };
    pEnumWindows(_enum_dbg_wnd_proc, (LPARAM)&ctx);
    return found;
}

/* --- Custom non-public anti-debug techniques (original research) --- */

/* 1. NtSetInformationThread(ThreadHideFromDebugger) via direct SSN extraction.
   Unlike public implementations that call ntdll!NtSetInformationThread directly,
   we extract the SSN and build a stub - the ntdll export is never called,
   making hooks on NtSetInformationThread ineffective.
   Class 0x11 = ThreadHideFromDebugger.
   When applied, the thread stops generating debug events (breakpoints, exceptions).
   A debugger attached after this call won't see thread activity. */
static void hide_thread_from_debugger() {
    HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
    if (!ntdll) return;
    void* pNtSIT = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtSetInformationThread"));
    if (!pNtSIT) return;
    DWORD ssn = get_ssn(pNtSIT);
    if (ssn == 0xFFFFFFFF) {
        void* neighbor = (void*)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtSetInformationKey"));
        ssn = get_ssn_fallback(pNtSIT, neighbor);
    }
    if (ssn == 0xFFFFFFFF) return;
    /* Stub from obfuscated bytes - no literal 4C 8B D1 B8 0F 05 C3 in .rdata (same pattern as direct_syscall) */
    static const unsigned char enc[] = {
        (unsigned char)(0x4C ^ (STUB_OBF_KEY + 0)), (unsigned char)(0x8B ^ (STUB_OBF_KEY + 1)),
        (unsigned char)(0xD1 ^ (STUB_OBF_KEY + 2)), (unsigned char)(0xB8 ^ (STUB_OBF_KEY + 3)),
        0u ^ (STUB_OBF_KEY + 4), (unsigned char)(0 ^ (STUB_OBF_KEY + 5)), (unsigned char)(0 ^ (STUB_OBF_KEY + 6)), (unsigned char)(0 ^ (STUB_OBF_KEY + 7)),
        (unsigned char)(0x0F ^ (STUB_OBF_KEY + 8)), (unsigned char)(0x05 ^ (STUB_OBF_KEY + 9)), (unsigned char)(0xC3 ^ (STUB_OBF_KEY + 10))
    };
    unsigned char stub[11];
    for (int i = 0; i < 11; i++) stub[i] = (unsigned char)(enc[i] ^ (STUB_OBF_KEY + i));
    *(DWORD*)(stub + 4) = ssn;
    void* exec = alloc_executable(sizeof(stub));
    if (!exec) return;
    memcpy(exec, stub, sizeof(stub));
    typedef NTSTATUS(NTAPI* NtSetInformationThread_t)(HANDLE, ULONG, PVOID, ULONG);
    auto fn = (NtSetInformationThread_t)exec;
    fn(GetCurrentThread(), 0x11, NULL, 0);
    VirtualFree(exec, 0, MEM_RELEASE);
}

/* 2. Kernel debugger detection via NtQuerySystemInformation(SystemKernelDebuggerInformation).
   Class 0x23 returns SYSTEM_KERNEL_DEBUGGER_INFORMATION { BOOLEAN DebuggerEnabled; BOOLEAN DebuggerNotPresent; }
   Detects WinDbg kernel-mode, HyperDbg, etc. Not commonly checked in user-mode anti-cheat.
   Class value from expression (no literal 0x23 in .rdata for scan). */
#define _SYSINFO_KERNEL_DEBUGGER_CLASS  ((ULONG)(0x20 + 3))
static bool kernel_debugger_attached() {
    typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
    static NtQuerySystemInformation_t NtQSI = nullptr;
    if (!NtQSI) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        NtQSI = (NtQuerySystemInformation_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQuerySystemInformation"));
        if (!NtQSI) return false;
    }
    struct { BOOLEAN Enabled; BOOLEAN NotPresent; } kdi = { 0, 0 };
    ULONG retLen = 0;
    if (NT_SUCCESS(NtQSI(_SYSINFO_KERNEL_DEBUGGER_CLASS, &kdi, sizeof(kdi), &retLen))) {
        if (kdi.Enabled && !kdi.NotPresent) return true;
    }
    return false;
}

/* 3. Debug object count check - NtQueryObject with ObjectAllTypesInformation (class 3).
   We scan for DebugObject type and check TotalNumberOfObjects.
   If any DebugObject exists in the system, a debugger is likely attached somewhere.
   This is non-trivial to spoof because it queries the kernel object manager. */
static bool debug_object_type_present() {
    typedef NTSTATUS(NTAPI* NtQueryObject_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static NtQueryObject_t NtQO = nullptr;
    if (!NtQO) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        NtQO = (NtQueryObject_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQueryObject"));
        if (!NtQO) return false;
    }
    ULONG bufSize = 0x10000;
    void* buf = VirtualAlloc(nullptr, bufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf) return false;
    ULONG retLen = 0;
    NTSTATUS st = NtQO(nullptr, 3, buf, bufSize, &retLen);
    if (st == 0xC0000004L && retLen > bufSize) {
        VirtualFree(buf, 0, MEM_RELEASE);
        bufSize = retLen + 0x1000;
        buf = VirtualAlloc(nullptr, bufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buf) return false;
        st = NtQO(nullptr, 3, buf, bufSize, &retLen);
    }
    bool found = false;
    if (NT_SUCCESS(st)) {
        /* OBJECT_TYPES_INFORMATION layout: NumberOfTypes (ULONG), then array of OBJECT_TYPE_INFORMATION. 
           Each entry: TypeName (UNICODE_STRING), then 10 ULONGs. Variable-size. */
        ULONG numTypes = *(ULONG*)buf;
        BYTE* ptr = (BYTE*)buf + sizeof(ULONG);
        ptr = (BYTE*)(((ULONG_PTR)ptr + 7) & ~7ULL);
        for (ULONG i = 0; i < numTypes && ptr < (BYTE*)buf + retLen - 64; i++) {
            USHORT nameLen = *(USHORT*)ptr;
            USHORT nameMaxLen = *(USHORT*)(ptr + 2);
            wchar_t* namePtr = *(wchar_t**)(ptr + 8);
            ULONG totalObjects = *(ULONG*)(ptr + 16);
            if (nameLen >= 20 && namePtr) {
                bool match = true;
                static const wchar_t target[] = { 'D','e','b','u','g','O','b','j','e','c','t',0 };
                for (int c = 0; target[c]; c++) {
                    if (c * 2 >= nameLen || namePtr[c] != target[c]) { match = false; break; }
                }
                if (match && totalObjects > 0) { found = true; break; }
            }
            ULONG entrySize = 16 + 10 * sizeof(ULONG) + nameMaxLen;
            entrySize = (entrySize + 7) & ~7U;
            ptr += entrySize;
        }
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    return found;
}

/* 4. NtQueryInformationProcess with ProcessInstrumentationCallback (class 0x28).
   When a debugger sets an instrumentation callback, class 0x28 returns non-null.
   This catches advanced debuggers that use instrumentation callbacks for tracing.
   Class value from expression (no literal 0x28 in .rdata for scan). */
#define _PROCINFO_INSTRUMENTATION_CALLBACK_CLASS  ((ULONG)(0x20 + 8))
static bool instrumentation_callback_set() {
    typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static NtQueryInformationProcess_t NtQIP = nullptr;
    if (!NtQIP) {
        HMODULE ntdll = api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll"));
        NtQIP = (NtQueryInformationProcess_t)api_resolve::get_proc_a(ntdll, APIRES_OBF_A("NtQueryInformationProcess"));
        if (!NtQIP) return false;
    }
    struct { ULONG Version; ULONG Reserved; void* Callback; } ici = { 0 };
    ULONG retLen = 0;
    if (NT_SUCCESS(NtQIP(GetCurrentProcess(), _PROCINFO_INSTRUMENTATION_CALLBACK_CLASS, &ici, sizeof(ici), &retLen))) {
        if (ici.Callback != nullptr) return true;
    }
    return false;
}

/* 5. Self-integrity: verify .text section hash hasn't been patched.
   Reads our own PE, finds .text, hashes first N bytes.
   On first call: stores baseline. Subsequent calls: compare.
   Breakpoints (0xCC/int3) or patches will change the hash.
   Uses FNV-1a (fast, no crypto dep). */
static bool text_section_tampered() {
    static DWORD stored_hash = 0;
    static DWORD stored_size = 0;
    void* base = peb_modules::get_image_base_address();
    if (!base) return false;
    auto* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = (IMAGE_NT_HEADERS*)((BYTE*)base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            BYTE* code = (BYTE*)base + sec->VirtualAddress;
            DWORD sz = min(sec->Misc.VirtualSize, 0x4000U);
            DWORD h = 0x811C9DC5;
            for (DWORD j = 0; j < sz; j++) {
                h ^= code[j]; h *= 0x01000193;
            }
            if (stored_hash == 0) { stored_hash = h; stored_size = sz; return false; }
            return (h != stored_hash);
        }
    }
    return false;
}

/* Initialize custom anti-debug protections. Call once at startup. */
static void _init_custom_anti_debug_impl() {
    hide_thread_from_debugger();
    text_section_tampered(); /* baseline capture */
}

/* Public API - NtQueryInfo + EnumWindows + custom checks. Defined in debugger_check.cpp. */
bool is_debugger_or_tool_detected();

void debugger_detection()
{
    while (true)
    {
        if (is_debugger_or_tool_detected()) { secure_exit(); }
        if (FileGrabHashCheck() || processhackerHashCheck() || processhacker2HashCheck() ||
            Xenosinjector64HashCheck() || XenosinjectorHashCheck() || ExtremeinjectorHashCheck() ||
            everthinghashcheck()) { secure_exit(); }
        if (detectInjectedDlls()) { Sleep(5000); secure_exit(); }
        if (DetectManualMappedDll()) { Sleep(5000); secure_exit(); }
        if (vmDetection::isVM()) { Sleep(5000); secure_exit(); }
        Sleep(2000);
    }
}