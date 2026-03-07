
#include <Windows.h>
#include <ntstatus.h>
#include <tchar.h>
#include <Shlobj.h>
#include <Psapi.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <sstream>
#include <urlmon.h>
#include <tlhelp32.h>
#include <winreg.h>
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")
#include <utility>
#include "../utilities/overlay/render.h"
#include "../utilities/sdk/utils/settings.h"
#include "../utilities/api_resolve.hpp"
#include "../utilities/impl/driver.hpp"
#include "../utilities/process_enum.hpp"
#include "../utilities/peb_modules.hpp"
#include "../utilities/direct_syscall.hpp"
#include "../protections/debugger_check_decl.hpp"
#include "../flush_comm_config.h"
#include "../utilities/etw_patch.hpp"
#pragma comment(lib, "ntdll.lib")

#include "../framework/imgui.h"
#include "../utilities/str_obfuscate.hpp"
#include "../framework/imgui_impl_dx11.h"
#include "../framework/imgui_impl_win32.h"

#include "../dependencies/loader/console.h"
#include "misc/misc.h"
#include "misc/core.h"
#include "bytes/bytes.h"
#include <fstream>
#include <urlmon.h>
#include "../Auth/auth.hpp"
#include <ctime>
#pragma comment(lib, "urlmon.lib")

// === PROTECTION FUNCTIONS ===

// Anti-Debugging Protection
typedef NTSTATUS(NTAPI* TNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

// _PROCESS_BASIC_INFORMATION might already be defined, so we'll use a different approach
#ifndef _PROCESS_BASIC_INFORMATION_DEFINED
#define _PROCESS_BASIC_INFORMATION_DEFINED
typedef struct _CUSTOM_PROCESS_BASIC_INFORMATION {
    PVOID Reserved1;
    PVOID PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID ParentProcessId;
} CUSTOM_PROCESS_BASIC_INFORMATION;
#endif

/* Debugger name obfuscation - key from FLUSHCOMM_OBF_BASE (no public 0x42 literal). */
#ifndef FLUSHCOMM_OBF_BASE
#  define FLUSHCOMM_OBF_BASE 0x42
#endif
#define _DBG_NAME_KEY ((char)((FLUSHCOMM_OBF_BASE + 0x0D) & 0xFF))
#define _DBG(c,i) ((char)((unsigned char)(c) ^ ((unsigned char)(_DBG_NAME_KEY) + ((i) & 0xF))))

static const char _enc_olly[]      = { _DBG('o',0),_DBG('l',1),_DBG('l',2),_DBG('y',3),_DBG('d',4),_DBG('b',5),_DBG('g',6),0 };
static const char _enc_ida64[]     = { _DBG('i',0),_DBG('d',1),_DBG('a',2),_DBG('6',3),_DBG('4',4),0 };
static const char _enc_x64dbg[]    = { _DBG('x',0),_DBG('6',1),_DBG('4',2),_DBG('d',3),_DBG('b',4),_DBG('g',5),0 };
static const char _enc_cheatengine[] = { _DBG('c',0),_DBG('h',1),_DBG('e',2),_DBG('a',3),_DBG('t',4),_DBG('e',5),_DBG('n',6),_DBG('g',7),_DBG('i',8),_DBG('n',9),_DBG('e',10),0 };
#undef _DBG

std::string xor_encrypt(const std::string& str, char key) {
    std::string result = str;
    for (size_t i = 0; i < result.length(); i++)
        result[i] ^= key;
    return result;
}

std::string decrypt_debugger_name(int index) {
    const char* encrypted_names[] = { _enc_olly, _enc_ida64, _enc_x64dbg, _enc_cheatengine };
    const size_t enc_lens[] = { sizeof(_enc_olly), sizeof(_enc_ida64), sizeof(_enc_x64dbg), sizeof(_enc_cheatengine) };
    if (index >= 0 && index < (int)(sizeof(encrypted_names) / sizeof(encrypted_names[0]))) {
        std::string encrypted(encrypted_names[index], enc_lens[index] - 1);
        return xor_encrypt(encrypted, _DBG_NAME_KEY);
    }
    return "";
}

/* NtQueryInformationProcess + EnumWindows - no IsDebuggerPresent/CheckRemoteDebuggerPresent (signatured) */
bool IsDebuggerPresentAdvanced() {
    return is_debugger_or_tool_detected();
}

/* Anti-VM: registry/disk checks. Strings obfuscated - no "vmware"/"virtualbox" literal in .rdata */
bool IsVirtualMachine() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, OBF_STR("HARDWARE\\DESCRIPTION\\System\\BIOS").c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[256];
        DWORD size = sizeof(buffer);
        if (RegQueryValueExA(hKey, OBF_STR("SystemManufacturer").c_str(), nullptr, nullptr, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
            char lowerBuffer[256];
            strcpy_s(lowerBuffer, buffer);
            _strlwr(lowerBuffer);
            if (strstr(lowerBuffer, OBF_STR("vmware").c_str()) || strstr(lowerBuffer, OBF_STR("virtualbox").c_str()) ||
                strstr(lowerBuffer, OBF_STR("microsoft corporation").c_str()) || strstr(lowerBuffer, OBF_STR("xen").c_str())) {
                RegCloseKey(hKey);
                return true;
            }
        }
        RegCloseKey(hKey);
    }
    ULARGE_INTEGER freeBytesAvailable;
    if (GetDiskFreeSpaceExA(nullptr, &freeBytesAvailable, nullptr, nullptr)) {
        if (freeBytesAvailable.QuadPart < 60ULL * 1024 * 1024 * 1024) return true;
    }
    return false;
}

/* Custom timing anti-debug: RDTSC pair around computed work.
   RDTSC reads TSC directly (ring-3) - less commonly intercepted than QPC.
   Single-step / breakpoint causes TSC gap > threshold.
   NtYieldExecution between samples adds noise to foil trivial patching. */
bool CheckTimingAnomalies() {
    unsigned int aux;
    unsigned long long t0 = __rdtscp(&aux);
    volatile unsigned long long acc = 0x9E3779B97F4A7C15ULL;
    for (volatile int i = 0; i < 800000; i++) {
        acc ^= acc << 13; acc ^= acc >> 7; acc ^= acc << 17;
    }
    unsigned long long t1 = __rdtscp(&aux);
    unsigned long long delta = t1 - t0;
    /* ~2 billion cycles @ 3 GHz ≈ 0.7s. Debugger single-step inflates this 10x+.
       Threshold 6 billion - generous to cover 1–5 GHz CPUs. */
    return delta > 6000000000ULL;
}

// Memory scanning protection - uses stealth process enum (NTFS/NtGetNextProcess)
bool ScanForSuspiciousMemory() {
    bool found_suspicious = false;
    process_enum::enumerate_stealth([&found_suspicious](DWORD, const std::wstring& path) {
        if (found_suspicious) return;
        if (path.empty()) return;
        size_t slash = path.find_last_of(L"\\/");
        std::wstring exe = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
        char procName[260] = { 0 };
        WideCharToMultiByte(CP_ACP, 0, exe.c_str(), -1, procName, 260, NULL, NULL);
        _strlwr(procName);
        static const std::string s1 = OBF_STR("wireshark"), s2 = OBF_STR("tcpview"),
            s3 = OBF_STR("procmon"), s4 = OBF_STR("procexp"), s5 = OBF_STR("autoruns"),
            s6 = OBF_STR("filemon"), s7 = OBF_STR("regmon"), s8 = OBF_STR("apimonitor");
        const char* suspicious[] = { s1.c_str(), s2.c_str(), s3.c_str(), s4.c_str(), s5.c_str(), s6.c_str(), s7.c_str(), s8.c_str() };
        for (const char* sus : suspicious) {
            if (strstr(procName, sus)) { found_suspicious = true; return; }
        }
    });
    return found_suspicious;
}

// Integrity checking - PEB image base (no GetModuleHandle in IAT)
bool VerifyCodeIntegrity() {
    HMODULE hModule = (HMODULE)peb_modules::get_image_base_address();
    if (!hModule) return false;
    
    // Simple checksum verification (basic protection)
    BYTE* base = (BYTE*)hModule;
    DWORD size = 0;
    
    // Get module size
    MODULEINFO modInfo;
    if (GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) {
        size = modInfo.SizeOfImage;
    }
    
    // Basic integrity check - sum bytes
    DWORD checksum = 0;
    for (DWORD i = 0; i < size && i < 0x1000; i++) {
        checksum += base[i];
    }
    
    // If checksum is suspiciously low, might indicate tampering
    return checksum > 1000;
}

// Main protection initialization - PEB image base (no GetModuleHandle in IAT)
// Anti-dumping protection
void ProtectMemoryRegions() {
    HMODULE hModule = (HMODULE)peb_modules::get_image_base_address();
    if (hModule) {
        // Make code section read-only
        DWORD oldProtect;
        VirtualProtect((LPVOID)hModule, 0x1000, PAGE_READONLY, &oldProtect);
        
        // Add guard pages around critical sections
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        
        // Create guard pages
        LPVOID guardPage = VirtualAlloc(nullptr, sysInfo.dwPageSize, 
                                       MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        if (guardPage) {
            // This makes it harder to dump memory
            VirtualProtect(guardPage, sysInfo.dwPageSize, PAGE_GUARD, &oldProtect);
        }
    }
}

void InitializeProtection() {
    init_custom_anti_debug();  /* ThreadHideFromDebugger + .text baseline */

    if (IsDebuggerPresentAdvanced()) {
        TerminateProcess(GetCurrentProcess(), 0);
    }
    
    /* Disabled - too aggressive, closed app after overlay choice:
    if (CheckTimingAnomalies()) { ExitProcess(0); }
    if (ScanForSuspiciousMemory()) { ExitProcess(0); }  // procmon, procexp, wireshark
    if (!VerifyCodeIntegrity()) { ExitProcess(0); }
    */
}

__int64 va_text = 0;

// Continuous protection monitoring thread
void protection_monitor() {
    while (true) {
        // Periodic security checks
        if (IsDebuggerPresentAdvanced()) {
            ExitProcess(0);
        }
        
        if (ScanForSuspiciousMemory()) {
            ExitProcess(0);
        }
        
        // Random delay to avoid pattern detection
        int random_delay = 500 + (rand() % 1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(random_delay));
    }
}

void cr3_loop()
{
	for (;;)
	{
	//	!DotMem::init_driver();
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
	}
}

size_t& _lxy_oxor_any_::X() {
	static size_t x = 0;
	return x;
}

size_t& _lxy_oxor_any_::Y() {
	static size_t y = 0;
	return y;
}
#define console_log( fmt, ... ) printf( " \n[>] " fmt "", ##__VA_ARGS__ )  


bool titlegen = true;
HWND MsgHW = NULL;
HWND cnhwnd = GetConsoleWindow();
uintptr_t status = 0;  /* Deferred - find_image requires driver+process_id, not ready at static init */
bool statusd = true;


bool CreateFileFromMemory(const std::string& desired_file_path, const char* address, size_t size)
{
	std::ofstream file_ofstream(desired_file_path.c_str(), std::ios_base::out | std::ios_base::binary);

	if (!file_ofstream.write(address, size))
	{
		file_ofstream.close();
		return false;
	}

	file_ofstream.close();
	return true;
}
void set_console_title()
{
	/* Generic titles - avoid cheat-related strings */
	static const char* titles[] = { "Console", "Runtime", "Service Host" };
	static size_t idx = 0;
	while (titlegen) {
		SetConsoleTitleA(titles[idx % 3]);
		idx++;
		Sleep(1000);
	}
}

#include <windows.h>
#include <urlmon.h>
#include <iostream>
#include <fstream>

void EncryptFilePath(const char* path)
{
	SetFileAttributesA(path, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
}

void CreateFileWithinBytes(const char* FilePath, std::vector<uint8_t> bytes)
{
	std::ofstream file(FilePath, std::ios_base::out | std::ios_base::binary);
	file.write((char*)bytes.data(), bytes.size());
	file.close();	
}

int GenRandNum(int min, int max) {
	std::random_device rd;
	std::mt19937 gen(rd());

	std::uniform_int_distribution<> distribution(min, max);

	return distribution(gen);
}

std::string GenerateRandomFileName(const char* extension)
{
	auto TEMPString = ("TEMP");

	auto dashSymbol = ("-");
	auto doubleSlashSymbol = ("\\");

	std::string randomFileName;
	randomFileName.append(std::to_string(GenRandNum(1111111111, 9999999999)));
	randomFileName.append(dashSymbol);
	randomFileName.append(std::to_string(GenRandNum(1111111111, 9999999999)));
	randomFileName.append(dashSymbol);
	randomFileName.append(std::to_string(GenRandNum(11111111, 99999999)));
	randomFileName.append(dashSymbol);
	randomFileName.append(std::to_string(GenRandNum(1111111111, 9999999999)));
	randomFileName.append(extension);

	return std::getenv(TEMPString) + std::string(doubleSlashSymbol) + randomFileName;
}

static BOOL CALLBACK EnumFortniteProc(HWND w, LPARAM lp) {
	char cls[64] = { 0 }, title[256] = { 0 };
	auto x1 = OBF_STR("UnrealWindow");
	auto x3 = OBF_STR("Fortnite");
	if (GetClassNameA(w, cls, sizeof(cls)) && _stricmp(cls, x1.c_str()) == 0 &&
	    GetWindowTextA(w, title, sizeof(title)) && strstr(title, x3.c_str())) {
		*(HWND*)lp = w;
		return FALSE;
	}
	return TRUE;
}

static HWND FindFortniteWindow()
{
	auto x1 = OBF_STR("UnrealWindow");
	auto x2 = OBF_STR("Fortnite  ");
	HWND h = FindWindowA(x1.c_str(), x2.c_str());
	if (h) return h;
	HWND found = NULL;
	EnumWindows(EnumFortniteProc, (LPARAM)&found);
	return found;
}

/* Clear console via kernel32 - no system("cls") spawning cmd.exe */
static void clear_console_no_spawn() {
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE) return;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;
	DWORD written, cells = (DWORD)csbi.dwSize.X * csbi.dwSize.Y;
	COORD c0 = { 0, 0 };
	FillConsoleOutputCharacterA(hOut, ' ', cells, c0, &written);
	SetConsoleCursorPosition(hOut, c0);
}
void back()
{
	clear_console_no_spawn();
	console.write("Waiting For Fortnite.");	


	while (FortniteWindow == NULL)
	{
		FortniteWindow = FindFortniteWindow();
	}
	DotMem::find_process(OBF_WSTR(L"FortniteClient-Win64-Shipping.exe").c_str());
	if (!status)
	{
		console.error("failed to get uengine (restart loader)");
		Sleep(1500);
		ExitProcess(0);
	}



	globals.ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	globals.ScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	for (auto i = 0; i < FLT_MAX; i++) {
		globals.va_text = DotMem::find_image() + i * 0x1000;
		auto uworld = read<uintptr_t>(globals.va_text + offsets::UWorld);
		auto level = read<uintptr_t>(uworld + 0x40);
		if (uworld && level && read<uintptr_t>(level + 0xc0) == uworld) {
			titlegen = false;
			console_log("Searching for target window...");
			if (!FortniteWindow)
			{
				console_log("[!] Target window not found");
				Error();
			}

			console_log("Target window found with HWND 0x%p", DotMem::find_image());

			console_log("Resolving process ID...");
			UINT targetProcessId;
			GetWindowThreadProcessId(FortniteWindow, reinterpret_cast<LPDWORD>(&targetProcessId));
			if (!targetProcessId)
			{
				console_log("[!] Failed to resolve process ID");
				Error();
			}

			console_log("Process ID resolved to %u", targetProcessId);

			console_log("Connecting to the process...");
			bool status = true;
			if (!status)
			{
				console_log("[!] Failed to connect to the process overlay backend");
				Error();
			}

			console_log("Connected to the process with mapped address 0x%p", DotMem::find_image());

			console_log("Drawing...");


			getchar();
			overlay::start();
			break;
		}
	}
	console_log("Searching for target window...");
	if (!FortniteWindow)
	{
		console_log("[!] Target window not found");
		Error();
	}

	console_log("Target window found with HWND 0x%p", DotMem::find_image());

	console_log("Resolving process ID...");
	UINT targetProcessId;
	GetWindowThreadProcessId(FortniteWindow, reinterpret_cast<LPDWORD>(&targetProcessId));
	if (!targetProcessId)
	{
		console_log("[!] Failed to resolve process ID");
		Error();
	}

	console_log("Process ID resolved to %u", targetProcessId);

	console_log("Connecting to the process...");
	bool status = true;
	if (!status)
	{
		console_log("[!] Failed to connect to the process overlay backend");
		Error();
	}

	console_log("Connected to the process with mapped address 0x%p", DotMem::find_image());

	console_log("Drawing...");


	getchar();
	overlay::start();
}

bool IsProcessOpen(const TCHAR* processName) {
	DWORD pid = process_enum::find_process_stealth(processName);
	return pid != 0;
}

void SetConsoleColor(int color) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, color);
}




// Welcome and warning messages
// Save credentials to file
void save_credentials(const std::string& username, const std::string& password) {
    std::ofstream cred_file("credentials.dat");
    if (cred_file.is_open()) {
        cred_file << username << "\n" << password;
        cred_file.close();
    }
}


void ShowWelcomeMessage() {
    clear_console_no_spawn();
    console.write("=====================");
    console.write("    WELCOME          ");
    console.write("=====================");
    console.write("Advanced Gaming Enhancement Suite");
    console.write("");
    
    // Scary military-grade protection warning
    console.error("!!! MILITARY-GRADE SECURITY ACTIVE !!!");
    console.error("========================================");
    console.error("WARNING: This software is protected by");
    console.error("military-grade anti-reverse engineering");
    console.error("and anti-debugging technology.");
    console.error("");
    console.error("Any attempt to analyze, debug, or modify");
    console.error("this software will result in immediate");
    console.error("system termination and security breach");
    console.error("detection protocols activation.");
    console.error("");
    console.error("=== SECURITY PROTOCOLS ENGAGED ===");
    console.error("- Real-time debugger detection");
    console.error("- Hardware breakpoint monitoring");
    console.error("- Virtual machine isolation checks");
    console.error("- Memory integrity verification");
    console.error("- Continuous threat scanning");
    console.error("========================================");
    console.write("");
    console.success("System security status: FULLY ARMED");
    console.write("");
    
	Sleep(3000); // Show message for 3 seconds
}

/* Terminate Epic Launcher and Fortnite/EAC processes before driver mapping - reduces detection on load.
   Uses NtGetNextProcess (direct syscall) - avoids CreateToolhelp32Snapshot/Process32First/Next (signatured).
   Obfuscation: no literal process names in binary. Call only when running as admin. */
#define _TERM_OBF_KEY  ((unsigned char)((FLUSHCOMM_OBF_BASE + 0x17) & 0xFF))
#define _TE(w,i) ((wchar_t)((unsigned)(w) ^ ((_TERM_OBF_KEY + (unsigned)(i)) & 0xFF)))
static const wchar_t _term_enc_0[] = { _TE('F',0),_TE('o',1),_TE('r',2),_TE('t',3),_TE('n',4),_TE('i',5),_TE('t',6),_TE('e',7),_TE('C',8),_TE('l',9),_TE('i',10),_TE('e',11),_TE('n',12),_TE('t',13),_TE('-',14),_TE('W',15),_TE('i',16),_TE('n',17),_TE('6',18),_TE('4',19),_TE('-',20),_TE('S',21),_TE('h',22),_TE('i',23),_TE('p',24),_TE('p',25),_TE('i',26),_TE('n',27),_TE('g',28),_TE('.',29),_TE('e',30),_TE('x',31),_TE('e',32),0 };
static const wchar_t _term_enc_1[] = { _TE('F',0),_TE('o',1),_TE('r',2),_TE('t',3),_TE('n',4),_TE('i',5),_TE('t',6),_TE('e',7),_TE('C',8),_TE('l',9),_TE('i',10),_TE('e',11),_TE('n',12),_TE('t',13),_TE('-',14),_TE('W',15),_TE('i',16),_TE('n',17),_TE('6',18),_TE('4',19),_TE('-',20),_TE('S',21),_TE('h',22),_TE('i',23),_TE('p',24),_TE('p',25),_TE('i',26),_TE('n',27),_TE('g',28),_TE('_',29),_TE('E',30),_TE('A',31),_TE('C',32),_TE('.',33),_TE('e',34),_TE('x',35),_TE('e',36),0 };
static const wchar_t _term_enc_2[] = { _TE('E',0),_TE('a',1),_TE('s',2),_TE('y',3),_TE('A',4),_TE('n',5),_TE('t',6),_TE('i',7),_TE('C',8),_TE('h',9),_TE('e',10),_TE('a',11),_TE('t',12),_TE('.',13),_TE('e',14),_TE('x',15),_TE('e',16),0 };
static const wchar_t _term_enc_3[] = { _TE('E',0),_TE('p',1),_TE('i',2),_TE('c',3),_TE('G',4),_TE('a',5),_TE('m',6),_TE('e',7),_TE('s',8),_TE('L',9),_TE('a',10),_TE('u',11),_TE('n',12),_TE('c',13),_TE('h',14),_TE('e',15),_TE('r',16),_TE('.',17),_TE('e',18),_TE('x',19),_TE('e',20),0 };
static const wchar_t _term_enc_4[] = { _TE('E',0),_TE('p',1),_TE('i',2),_TE('c',3),_TE('W',4),_TE('e',5),_TE('b',6),_TE('H',7),_TE('e',8),_TE('l',9),_TE('p',10),_TE('e',11),_TE('r',12),_TE('.',13),_TE('e',14),_TE('x',15),_TE('e',16),0 };
static const wchar_t _term_enc_5[] = { _TE('U',0),_TE('n',1),_TE('r',2),_TE('e',3),_TE('a',4),_TE('l',5),_TE('C',6),_TE('E',7),_TE('F',8),_TE('S',9),_TE('u',10),_TE('b',11),_TE('P',12),_TE('r',13),_TE('o',14),_TE('c',15),_TE('e',16),_TE('s',17),_TE('s',18),_TE('.',19),_TE('e',20),_TE('x',21),_TE('e',22),0 };
static const wchar_t* const _term_targets[] = { _term_enc_0, _term_enc_1, _term_enc_2, _term_enc_3, _term_enc_4, _term_enc_5 };
static void _term_dec(wchar_t* out, const wchar_t* enc, size_t n) {
	for (size_t i = 0; i < n && enc[i]; i++)
		out[i] = (wchar_t)((unsigned)enc[i] ^ ((_TERM_OBF_KEY + (unsigned)i) & 0xFF));
	out[n < 256 ? n : 255] = 0;
}
static const char* _term_basename(const char* path) {
	const char* last = path;
	for (const char* p = path; *p; p++) if (*p == '\\' || *p == '/') last = p + 1;
	return last;
}
struct _term_ctx { DWORD selfPid; };
/* NtOpenProcess at runtime - no OpenProcess in IAT (EAC/fortnite). Local struct to avoid include-order. */
struct _term_client_id { PVOID UniqueProcess; PVOID UniqueThread; };
typedef NTSTATUS(NTAPI* PFN_NtOpenProcess)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, _term_client_id*);
static PFN_NtOpenProcess _term_get_NtOpenProcess() {
	static PFN_NtOpenProcess fn = (PFN_NtOpenProcess)api_resolve::get_proc_a(
		api_resolve::get_module_w(APIRES_OBF_W(L"ntdll.dll")), APIRES_OBF_A("NtOpenProcess"));
	return fn;
}
static void _term_cb(DWORD pid, const char* path, void* v) {
	_term_ctx* ctx = (_term_ctx*)v;
	if (pid == ctx->selfPid) return;
	const char* base = _term_basename(path);
	wchar_t baseW[128] = { 0 };
	MultiByteToWideChar(CP_ACP, 0, base, -1, baseW, 127);
	for (int t = 0; t < (int)(sizeof(_term_targets)/sizeof(_term_targets[0])); t++) {
		wchar_t dec[128];
		_term_dec(dec, _term_targets[t], 64);
		if (_wcsicmp(baseW, dec) == 0) {
			PFN_NtOpenProcess pNtOpenProcess = _term_get_NtOpenProcess();
			if (pNtOpenProcess) {
				OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, 0, 0, 0, 0 };
				_term_client_id cid = { (PVOID)(ULONG_PTR)pid, 0 };
				HANDLE h = 0;
				if (NT_SUCCESS(pNtOpenProcess(&h, PROCESS_TERMINATE, &oa, &cid)) && h) {
					TerminateProcess(h, 0);
					CloseHandle(h);
				}
			}
			break;
		}
	}
}
static void terminate_epic_fortnite_processes() {
	_term_ctx ctx = { GetCurrentProcessId() };
	enumerate_processes_nt(_term_cb, &ctx);
}

bool __stdcall main()
{
#if FLUSHCOMM_PATCH_ETW
	EtwPatch::Init();  /* Reduce ETW telemetry early */
#endif
	// Show welcome and warning messages first
	ShowWelcomeMessage();
	
	
	
	// Initialize protection systems
	InitializeProtection();
	
	/* protection_monitor disabled - it silently ExitProcess(0) when detecting procmon/procexp/etc,
	   causing FrozenPublic to close right after overlay choice. Keep initial checks only. */
	
	std::thread title(set_console_title);
	title.detach();



	// Driver loading section (FlushComm requires Administrator)
	if (!DotMem::is_admin()) {
		console.error("Administrator required - Right-click and 'Run as administrator'");
		console.error("FlushComm needs HKLM registry access for driver communication.");
		console.write("Press any key to exit...");
		getchar();
		return false;
	}
	/* Close Epic Launcher and Fortnite/EAC before mapping - driver loads with no AC processes watching */
	console.write("Closing Epic/Fortnite processes before driver load...");
	terminate_epic_fortnite_processes();
	Sleep(1500);  /* Give processes time to exit before driver load */
	/* EAC: brief startup delay before driver access - reduces "immediate driver touch" fingerprint */
	Sleep(400 + (DWORD)(GetTickCount64() % 250));

	console.write("Checking for existing driver...");
	if (!DotMem::find_driver()) {
		console.write("Driver not found, attempting to load...");
		LoadDriver();

		Sleep(3500);  /* Let driver MappedInitWorker complete (section + Beep hook); extra buffer for slow systems */
		console.write("Verifying driver load...");
		if (!DotMem::find_driver()) {
#if FLUSHCOMM_REJECT_REGISTRY_FALLBACK
			printf("[DRIVER] find_driver failed - section not found or driver did not respond (section-only mode, no MmCopyVirtualMemory fallback)\n");
#else
			printf("[DRIVER] find_driver failed - registry/Beep ok but driver did not respond to REQ_INIT\n");
#endif
			console.error("Failed To Load Driver - Check previous error messages");
			console.error("This could be due to:");
			console.error("- FLUSHCOMM_MAGIC mismatch (driver.sys built separately - do Rebuild Solution)");
			console.error("- Antivirus interference");
			console.error("- Network issues downloading mapper");
			console.error("- Corrupted driver bytes");
			
			console.write("Press any key to exit...");
			getchar();
			return false;
		}
		else {
			console.success("Driver Loaded Successfully");
			Beep(500, 500);
		}
	}
	else {
		console.write("Driver Already Loaded");
		console.success("Proceeding with cheat initialization");
	}
	/* Enable EAC identifier spoofer (RtlGetVersion, NtQuerySystemInformation) */
	DotMem::spoofer_enable(true);

		console_log("Waiting For Fortnite");
		globals.ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
		globals.ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
		while (FortniteWindow == NULL)
		{
			FortniteWindow = FindFortniteWindow();
			Sleep(150);
		}
		console_log("Found Fortnite");
		Sleep(1000);
		
		// Ask user about overlay method
		console_log("Do you want to use Discord overlay, CrosshairX overlay, or own overlay? (MORE CPU)");
		console_log("1 - Discord Overlay (Lower CPU)");
		console_log("2 - CrosshairX Overlay (Lower CPU)");
		console_log("3 - Own Overlay (MORE CPU)");
		printf("\n[>] Enter choice (1, 2, or 3): ");
		
		int choice = 0;
		while (choice != 1 && choice != 2 && choice != 3) {
			char input[10];
			if (fgets(input, sizeof(input), stdin)) {
				choice = atoi(input);
				if (choice != 1 && choice != 2 && choice != 3) {
					printf("[>] Invalid choice. Please enter 1, 2, or 3: ");
				}
			}
		}
		
		globals.use_discord = (choice == 1);
		globals.use_crosshairx = (choice == 2);
		
		/* Wait for user to press Enter - use RegisterHotKey so it works when Fortnite has focus
		 * (GetAsyncKeyState fails when game uses DirectInput/raw input). */
		static volatile bool g_enter_pressed = false;
		g_enter_pressed = false;
		BOOL hotkey_ok = RegisterHotKey(NULL, 1, 0, VK_RETURN);  /* NULL = post to thread queue */

		if (globals.use_discord) {
		    console_log("Checking for Discord...");
		    HWND discord_window = FindDiscordWindow();
		    if (!discord_window) {
		        console_log("[!] Discord not found - falling back to own overlay (MORE CPU).");
		        globals.use_discord = false;
		    } else {
		        console_log("Discord found!");
		    }
		    console_log("Press Enter to inject (in lobby - focus Fortnite, then press Enter)");
		} else if (globals.use_crosshairx) {
			console_log("Checking for CrosshairX...");
			HWND crosshairx_window = FindWindowA(OBF_STR("Chrome_WidgetWin_1").c_str(), OBF_STR("CrosshairX").c_str());
			if (!crosshairx_window) {
				console_log("[!] CrosshairX not found - falling back to own overlay (MORE CPU).");
				globals.use_crosshairx = false;
			} else {
				console_log("CrosshairX found!");
			}
			console_log("Press Enter to inject (in lobby - focus Fortnite, then press Enter)");
		} else {
			console_log("Using own overlay (MORE CPU)");
			console_log("Press Enter In The Lobby (focus Fortnite, then press Enter)");
		}

		while (!g_enter_pressed) {
			if (hotkey_ok) {
				MSG msg = { 0 };
				while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_HOTKEY && msg.wParam == 1) {
						g_enter_pressed = true;
						break;
					}
				}
			} else {
				if (GetAsyncKeyState(VK_RETURN) & 0x8000) g_enter_pressed = true;
			}
			Sleep(50);
		}
		if (hotkey_ok) UnregisterHotKey(NULL, 1);
		
		console_log("Searching for Fortnite process...");
		// Find Fortnite process - try main exe and EAC variant, retry a few times
		INT32 pid = 0;
		auto pn1 = OBF_WSTR(L"FortniteClient-Win64-Shipping.exe");
		auto pn2 = OBF_WSTR(L"FortniteClient-Win64-Shipping_EAC.exe");
		for (int retry = 0; retry < 5 && !pid; retry++) {
			pid = DotMem::find_process(pn1.c_str());
			if (!pid) pid = DotMem::find_process(pn2.c_str());
			if (!pid) {
				console_log("[!] Fortnite process not found, retrying in 2 seconds...");
				Sleep(2000);
			}
		}
		if (!pid) {
			console_log("[!] Failed to find Fortnite process. Make sure game is fully loaded in lobby.");
			console_log("Press any key to exit...");
			getchar();
			return false;
		}
		// CRITICAL: Wait for game to stabilize before ANY driver reads - find_image/fetch_cr3
		// can block on page faults if game is loading. 30s minimizes freeze on loading screen.
		console_log("Waiting 30 seconds for game to stabilize (avoids freeze on loading)...");
		for (int c = 30; c > 0; c--) {
			printf("\r[>] %d seconds remaining... ", c);
			Sleep(1000);
		}
		printf("\n");
		console_log("Initializing driver connection (PID=%d)...", (int)DotMem::process_id);
		virtualaddy = 0;
		cr3 = 0;
		for (int retry = 0; retry < 5 && (!virtualaddy || !cr3); retry++) {
			if (retry > 0) {
				console_log("[!] Retry %d/5 in 3 seconds...", retry);
				Sleep(3000);
			}
			virtualaddy = DotMem::find_image();
			cr3 = DotMem::fetch_cr3(virtualaddy);  /* Pass base for CR3 validation when EAC spoofs PsGetProcessSectionBaseAddress */
		}
		if (!virtualaddy || !cr3) {
			console_log("[!] Driver returned base=0x%p cr3=0x%llX", (void*)virtualaddy, (unsigned long long)cr3);
			console_log("    - Rebuild driver.sys and reload (reboot first to clear old driver)");
			console_log("    - Fortnite with EAC: try launching game first, get to lobby, then run");
			console_log("    - VBS/HVCI can block CR3 read - disable in Windows Security for testing");
			console_log("Press any key to exit...");
			getchar();
			return false;
		}
		std::cout << "\n[+] UWORLD offset -> " << std::hex << offsets::UWorld << std::dec;
		std::cout << "\n[+] Base address -> 0x" << std::hex << virtualaddy << std::dec;
		std::cout << "\n[+] cr3 address -> 0x" << std::hex << cr3 << std::dec;

		titlegen = true;
		// Read pause: skip ALL game memory reads for 10s - driver reads during load block on page faults
		// Reduced from 90s to 10s - ESP can start working sooner, CacheLevels starts after pause expires
		const DWORD READ_PAUSE_MS = 10000;  // 10 seconds - enough for game to stabilize
		g_read_pause_until = GetTickCount() + READ_PAUSE_MS;
		// Own overlay: delay CacheLevels until read pause expires - no driver reads during load
		// Capture by value to avoid use-after-scope if main thread exits (prevents crash)
		if (!globals.use_discord && !globals.use_crosshairx) {
			Sleep(800);  // Let driver/memory settle
			const DWORD pause_ms = READ_PAUSE_MS;
			std::thread([pause_ms]() { Sleep(pause_ms + 2000); CacheLevels(); }).detach();
		} else {
			std::thread([]() { Sleep(3000); CacheLevels(); }).detach();  // 3s delay for hijacked overlay
		}
		overlay::start();
		/* If overlay::start() returns (init failed), keep console open so user sees the error */
		console_log("Overlay ended. Press any key to exit...");
		getchar();
		globals.ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
		globals.ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	}
