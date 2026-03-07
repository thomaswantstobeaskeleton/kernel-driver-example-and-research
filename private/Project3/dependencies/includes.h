#pragma once

#include <windows.h>
#include "../utilities/sdk/render/sdk.h"
#include "../framework/imgui.h"
#include <d3d9.h>
#include "../framework/imgui_impl_win32.h"
#include "../utilities/impl/driver.hpp"
#include <dwmapi.h>
#include <tchar.h>
/* skStr removed - use str_obfuscate (OBF_STR) to avoid public xorstr/skCrypt signatures */
#include "configs/mapper_config.h"
#include "configs/driver_mapper_selection.h"
#include <tlhelp32.h>
#include "../utilities/peb_modules.hpp"
#include "../utilities/str_obfuscate.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <mutex>
#include <fstream>
#include "../dependencies/loader/console.h"
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

#define console_log( fmt, ... ) console_log( " [>] " fmt "", ##__VA_ARGS__ )

class structs
{
public:

	uintptr_t
		UWorld,
		GameInstance,
		GameState,
		LocalPlayer,
		AcknownledgedPawn,
		PlayerState,
		PlayerController,
		RootComponent,
		Mesh,
		PlayerArray,
		LocalWeapon;
	int32_t
		AmmoCount;

	int
		TeamIndex,
		PlayerArraySize;


}; structs CachePointers;

// Read pause: when GetTickCount() < this, skip ALL game memory reads (prevents freeze during load)
// Driver reads on transitioning memory can block on page faults - avoid during load
extern DWORD g_read_pause_until;

fvector RelativeLocation;

class entity {
public:
	uintptr_t
		entity,
		skeletal_mesh,
		root_component,
		player_state;


	char
		IgnoreDeads;

	int
		team_index,
		kills;
	char
		team_number;
	float
		lastrendertime;
	bool
		is_visible;
};
std::vector<entity> entity_list;
std::vector<entity> temporary_entity_list;

class item {
public:
	uintptr_t
		Actor;

	std::string
		Name;
	bool
		isVehicle,
		isChest,
		isPickup,
		isAmmoBox;
	float
		distance;
};

std::vector<item> item_pawns;

// Anti-DLL Injection
struct ModuleInfo {
	std::string name;
	DWORD64 baseAddress;
	DWORD size;
};

static std::vector<ModuleInfo> legitimate_modules;
static std::mutex modules_mutex;

static const std::vector<std::string> system_dlls = {
	"KERNEL32.DLL", "KERNELBASE.DLL", "NTDLL.DLL", "USER32.DLL",
	"WIN32U.DLL", "GDI32.DLL", "GDI32FULL.DLL", "ADVAPI32.DLL",
	"MSVCRT.DLL", "SECHOST.DLL", "RPCRT4.DLL", "CRYPTBASE.DLL",
	"BCRYPTPRIMITIVES.DLL", "CRYPTSP.DLL", "SSPICLI.DLL", "CRYPT32.DLL",
	"MSASN1.DLL", "WLDAP32.DLL", "FLTLIB.DLL", "WS2_32.DLL",
	"OLEAUT32.DLL", "OLE32.DLL", "SHELL32.DLL", "SHLWAPI.DLL",
	"SETUPAPI.DLL", "CFGMGR32.DLL", "POWRPROF.DLL", "UMPDC.DLL",
	"VCRUNTIME140.DLL", "VCRUNTIME140_1.DLL", "MSVCP140.DLL",
	"CONCRT140.DLL", "D3D11.DLL", "DXGI.DLL", "urlmon.dll", "wininet.dll",
	"key.dll", "keyauthpatcher.dll", "patcher.dll"
};

static inline bool is_system_dll(const std::string& dll_name) {
	std::string upper_dll_name = dll_name;
	std::transform(upper_dll_name.begin(), upper_dll_name.end(), upper_dll_name.begin(), ::toupper);
	return std::find(system_dlls.begin(), system_dlls.end(), upper_dll_name) != system_dlls.end();
}

static inline bool is_dll_in_system_directory(const std::string& dll_name) {
	char system_dir[MAX_PATH];
	char system32_dir[MAX_PATH];

	if (GetSystemDirectoryA(system_dir, MAX_PATH)) {
		std::string dll_path = std::string(system_dir) + "\\" + dll_name;
		if (GetFileAttributesA(dll_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
			return true;
		}
	}

	if (GetSystemWow64DirectoryA(system32_dir, MAX_PATH)) {
		std::string dll_path = std::string(system32_dir) + "\\" + dll_name;
		if (GetFileAttributesA(dll_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
			return true;
		}
	}

	return false;
}

static inline bool is_main_executable(const std::string& module_name) {
	std::string selfName = peb_modules::get_self_module_name();
	const char* exeName = strrchr(selfName.c_str(), '\\');
	exeName = exeName ? exeName + 1 : selfName.c_str();
	return _stricmp(module_name.c_str(), exeName) == 0;
}

static inline bool is_legitimate_module(const std::string& module_name, DWORD64 base_address) {
	if (is_main_executable(module_name)) {
		return true;
	}

	if (is_system_dll(module_name) || is_dll_in_system_directory(module_name)) {
		return true;
	}

	std::lock_guard<std::mutex> lock(modules_mutex);
	return std::any_of(legitimate_modules.begin(), legitimate_modules.end(),
		[&](const ModuleInfo& info) {
			return _stricmp(info.name.c_str(), module_name.c_str()) == 0 &&
				info.baseAddress == base_address;
		});
}

/* PEB-based module enumeration - avoids CreateToolhelp32Snapshot (monitored) */
static inline void initialize_legitimate_modules() {
	std::lock_guard<std::mutex> lock(modules_mutex);
	legitimate_modules.clear();

	peb_modules::ModuleInfo pebMods[512];
	int n = peb_modules::enumerate_modules(pebMods, 512);
	for (int i = 0; i < n; i++) {
		char moduleName[MAX_PATH] = { 0 };
		size_t convertedChars = 0;
		wcstombs_s(&convertedChars, moduleName, MAX_PATH, pebMods[i].name, MAX_PATH);
		const char* baseName = strrchr(moduleName, '\\');
		baseName = baseName ? baseName + 1 : moduleName;
		legitimate_modules.push_back({
			std::string(baseName),
			(DWORD64)pebMods[i].base,
			(DWORD)pebMods[i].size
		});
	}
}

static void anti_dll_injection() {
	initialize_legitimate_modules();

	while (true) {
		peb_modules::ModuleInfo pebMods[512];
		int n = peb_modules::enumerate_modules(pebMods, 512);
		for (int i = 0; i < n; i++) {
			char moduleName[MAX_PATH] = { 0 };
			size_t convertedChars = 0;
			wcstombs_s(&convertedChars, moduleName, MAX_PATH, pebMods[i].name, MAX_PATH);
			const char* baseName = strrchr(moduleName, '\\');
			baseName = baseName ? baseName + 1 : moduleName;
			if (!is_legitimate_module(baseName, (DWORD64)pebMods[i].base)) {
				TerminateProcess(GetCurrentProcess(), 0);
			}
		}
		Sleep(1000);
	}
}

void init_security_features()
{
	std::thread([]() { anti_dll_injection(); }).detach();
}

void LoadDriver()
{
	printf("[DRIVER] Starting driver loading process...\n");
	
	/* Use YOUR locally compiled driver - no download. Need full path to derive exe directory. */
	std::string selfPath = peb_modules::get_self_module_full_path();
	if (selfPath.empty()) {
		/* PEB path can be empty on some setups; fallback so mapper/driver always get correct exe dir. */
		char exePathBuf[MAX_PATH] = { 0 };
		if (GetModuleFileNameA(GetModuleHandleA(NULL), exePathBuf, MAX_PATH) > 0)
			selfPath = exePathBuf;
		else
			selfPath = peb_modules::get_self_module_name();
	}
	char exePath[MAX_PATH] = { 0 };
	strncpy_s(exePath, selfPath.c_str(), MAX_PATH - 1);
	std::string exeDir = exePath;
	size_t lastSlash = exeDir.find_last_of("\\/");
	if (lastSlash != std::string::npos) {
		exeDir = exeDir.substr(0, lastSlash + 1);
	} else {
		exeDir = ".\\";
	}
	/* Resolve to absolute path so CreateProcess child gets correct cwd and paths work regardless of loader cwd. */
	char exeDirAbs[MAX_PATH] = { 0 };
	if (GetFullPathNameA(exeDir.c_str(), MAX_PATH, exeDirAbs, NULL) > 0 && exeDirAbs[0])
		exeDir = exeDirAbs;
	/* Ensure trailing backslash for concatenation */
	if (!exeDir.empty() && exeDir.back() != '\\' && exeDir.back() != '/')
		exeDir += '\\';

	std::string driverPath = exeDir + OBF_STR("driver.sys");
	std::string mapperPath;
	/* Mapper selection: 0=kdmapper 1=Aether.Mapper 2=rtcore 3=LegitMemory (untested) */
	int mapperType = (g_mapper_type >= 0 && g_mapper_type <= 3) ? g_mapper_type : 0;
	std::string mapperExe[] = { OBF_STR("kdmapper.exe"), OBF_STR("Aether.Mapper.exe"), OBF_STR("rtcore_mapper.exe"), OBF_STR("LegitMemory.exe") };
	std::string mapperDir = exeDir;
	if (!g_mapper_directory.empty()) {
		mapperDir = g_mapper_directory;
		if (mapperDir.back() != '\\' && mapperDir.back() != '/')
			mapperDir += '\\';
	}
	mapperPath = mapperDir + mapperExe[mapperType];

	/* Fallback paths: same dir (kdmapper build copies here), then driver build output */
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "driver.sys";  /* x64/Release - kdmapper and driver output same folder */
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "..\\driver\\x64\\Release\\driver.sys";  /* Project3/x64/Release -> driver/x64/Release */
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "..\\Project3\\driver\\build\\driver\\driver.sys";
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "..\\driver\\build\\driver\\driver.sys";  /* Project3/x64/Release -> driver/build/driver/ */
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "..\\..\\Project3\\driver\\build\\driver\\driver.sys";
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "undetecteddrv\\x64\\Release\\driver.sys";
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "..\\undetecteddrv\\x64\\Release\\driver.sys";
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "..\\..\\undetecteddrv\\x64\\Release\\driver.sys";
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "build\\driver\\driver.sys";
	}
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		driverPath = exeDir + "..\\build\\driver\\driver.sys";
	}
	
	/* Mapper fallbacks: try other mappers if selected one not found */
	if (GetFileAttributesA(mapperPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		for (int i = 0; i < 4; i++) {
			if (i == mapperType) continue;
			mapperPath = exeDir + mapperExe[i];
				if (GetFileAttributesA(mapperPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
				mapperType = i;
				printf("[DRIVER] Selected mapper not found, using %s\n", mapperExe[i].c_str());
				break;
			}
		}
	}
	if (GetFileAttributesA(mapperPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		mapperPath = exeDir + OBF_STR("mapper.exe");
	}
	if (GetFileAttributesA(mapperPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		mapperPath = exeDir + OBF_STR("..\\..\\x64\\Release\\kdmapper.exe");
	}
	if (GetFileAttributesA(mapperPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		mapperPath = "C:\\Windows\\INF\\mapper_temp.exe";
		printf("[DRIVER] Downloading mapper (local not found)...\n");
		HRESULT hr = URLDownloadToFileA(NULL,
#ifdef MAPPER_DOWNLOAD_URL
			MAPPER_DOWNLOAD_URL
#else
			OBF_STR("https://files.catbox.moe/wt2bfo.bin").c_str()
#endif
			, mapperPath.c_str(), 0, NULL);
		if (hr != S_OK) {
			printf("[DRIVER] Failed to download mapper! Place mapper.exe in exe folder.\n");
			return;
		}
		printf("[DRIVER] Mapper downloaded\n");
	}
	
	/* Resolve to full paths so kdmapper sees absolute paths and finds driver regardless of cwd. */
	char driverPathFull[MAX_PATH] = { 0 }, mapperPathFull[MAX_PATH] = { 0 };
	if (GetFullPathNameA(driverPath.c_str(), MAX_PATH, driverPathFull, NULL) > 0 && driverPathFull[0])
		driverPath = driverPathFull;
	if (GetFullPathNameA(mapperPath.c_str(), MAX_PATH, mapperPathFull, NULL) > 0 && mapperPathFull[0])
		mapperPath = mapperPathFull;

	printf("[DRIVER] Using driver: %s\n", driverPath.c_str());
	printf("[DRIVER] Using mapper: %s\n", mapperPath.c_str());
	
	if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		printf("[DRIVER] Driver not found! Build the driver project and place driver.sys in exe folder.\n");
		return;
	}
	if (GetFileAttributesA(mapperPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		printf("[DRIVER] Mapper not found!\n");
		return;
	}

	/* If Intel driver device is already present, NtLoadDriver will always return 0xC0000033. Check before running kdmapper. */
	{
		HANDLE hNal = CreateFileW(L"\\\\.\\Nal", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (hNal != INVALID_HANDLE_VALUE) {
			CloseHandle(hNal);
			printf("[DRIVER] Intel driver device (\\\\.\\Nal) is already present - mapper would get 0xC0000033.\n");
			printf("[DRIVER] Disable: (1) Windows Security -> Device Security -> Core Isolation = Off. (2) Registry: HKLM\\SYSTEM\\CurrentControlSet\\Control\\CI\\Config, set VulnerableDriverBlocklistEnable = 0 (DWORD). Then reboot.\n");
			return;
		}
	}

	/* Unload leftover Intel vulnerable driver(s) to avoid 0xC0000033. Enable SeLoadDriverPrivilege so NtUnloadDriver succeeds. */
	{
		typedef struct _USTR { USHORT Length; USHORT MaximumLength; wchar_t* Buffer; } USTR;
		typedef LONG (NTAPI *FN_Unload)(USTR*);
		HMODULE ntdll = GetModuleHandleA("ntdll.dll");
		FN_Unload pUnload = ntdll ? (FN_Unload)GetProcAddress(ntdll, "NtUnloadDriver") : NULL;
		if (pUnload) {
			TOKEN_PRIVILEGES tp = { 1, { 0, 0, SE_PRIVILEGE_ENABLED } };
			HANDLE hTok = NULL;
			if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hTok)) {
				if (LookupPrivilegeValueW(NULL, L"SeLoadDriverPrivilege", &tp.Privileges[0].Luid))
					AdjustTokenPrivileges(hTok, FALSE, &tp, 0, NULL, NULL);
				CloseHandle(hTok);
			}
			Sleep(150);
			/* 1) Unload fixed name iqvw64e – retry a few times so kernel can release device (\Device\Nal) */
			static const wchar_t iqvw[] = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\iqvw64e";
			USTR us1 = { (USHORT)(sizeof(iqvw) - sizeof(wchar_t)), (USHORT)sizeof(iqvw), (wchar_t*)iqvw };
			for (int u = 0; u < 3; u++) {
				LONG st = pUnload(&us1);
				if (st == 0 || st == (LONG)0xC0000034) break;
				Sleep(300);
			}
			HKEY hSvcRoot = NULL;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_ALL_ACCESS, &hSvcRoot) == ERROR_SUCCESS) {
				RegDeleteTreeW(hSvcRoot, L"iqvw64e");
				RegCloseKey(hSvcRoot);
			}
			Sleep(200);
			/* 2) Enumerate and unload leftover random-name services (old kdmapper: 12–30 alphanumeric, ImagePath in Temp) */
			HKEY hSvc = NULL;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS | KEY_WRITE, &hSvc) == ERROR_SUCCESS) {
				wchar_t keyName[256];
				DWORD idx = 0, cleaned = 0;
				while (cleaned < 8 && RegEnumKeyW(hSvc, idx, keyName, 256) == ERROR_SUCCESS) {
					DWORD len = (DWORD)wcslen(keyName);
					if (len >= 12 && len <= 30) {
						int alnum = 1;
						for (DWORD i = 0; i < len && alnum; i++) {
							wchar_t c = keyName[i];
							alnum = ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9'));
						}
						if (alnum) {
							HKEY hSub = NULL;
							if (RegOpenKeyExW(hSvc, keyName, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
								wchar_t pathBuf[512] = { 0 };
								DWORD pathType = 0, pathLen = sizeof(pathBuf);
								if (RegQueryValueExW(hSub, L"ImagePath", NULL, &pathType, (LPBYTE)pathBuf, &pathLen) == ERROR_SUCCESS && wcsstr(pathBuf, L"Temp")) {
									RegCloseKey(hSub);
									wchar_t fullPath[320];
									swprintf_s(fullPath, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\%s", keyName);
									USTR us = { (USHORT)((wcslen(fullPath)) * sizeof(wchar_t)), (USHORT)((wcslen(fullPath) + 1) * sizeof(wchar_t)), fullPath };
									(void)pUnload(&us);
									RegDeleteTreeW(hSvc, keyName);
									cleaned++;
									idx = 0;
									continue;
								}
								RegCloseKey(hSub);
							}
						}
					}
					idx++;
				}
				/* 2b) Unload ANY service whose ImagePath contains iqvw64e (case-insensitive). Catches legitimate Intel Network Adapter driver (creates \\Device\\Nal) so we can load our copy. */
				idx = 0;
				cleaned = 0;
				while (cleaned < 8 && RegEnumKeyW(hSvc, idx, keyName, 256) == ERROR_SUCCESS) {
					HKEY hSub = NULL;
					if (RegOpenKeyExW(hSvc, keyName, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
						wchar_t pathBuf[512] = { 0 };
						DWORD pathType = 0, pathLen = sizeof(pathBuf);
						if (RegQueryValueExW(hSub, L"ImagePath", NULL, &pathType, (LPBYTE)pathBuf, &pathLen) == ERROR_SUCCESS && pathBuf[0]) {
							wchar_t pathLower[512];
							wcsncpy_s(pathLower, pathBuf, _TRUNCATE);
							_wcslwr_s(pathLower);
							if (wcsstr(pathLower, L"iqvw64e")) {
								RegCloseKey(hSub);
								wchar_t fullPath[320];
								swprintf_s(fullPath, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\%s", keyName);
								USTR us = { (USHORT)((wcslen(fullPath)) * sizeof(wchar_t)), (USHORT)((wcslen(fullPath) + 1) * sizeof(wchar_t)), fullPath };
								for (int u = 0; u < 2; u++) {
									LONG st = pUnload(&us);
									if (st == 0 || st == (LONG)0xC0000034) break;
									Sleep(200);
								}
								RegDeleteTreeW(hSvc, keyName);
								cleaned++;
								idx = 0;
								Sleep(300);
								continue;
							}
						}
						RegCloseKey(hSub);
					}
					idx++;
				}
				RegCloseKey(hSvc);
			}
			Sleep(300);
		}
	}
	
	/* Run selected mapper with appropriate command line. Capture stdout/stderr. */
	PROCESS_INFORMATION pi = { 0 };
	int result = -1;
	std::string mapperOutput;

	{
		auto runMapperWithCmd = [&](const std::string& cmdLine) -> bool {
			std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
			cmdBuf.push_back('\0');
			mapperOutput.clear();
			std::string mapperCwd = exeDir;
			{
				size_t pos = mapperPath.find_last_of("\\/");
				if (pos != std::string::npos && pos > 0)
					mapperCwd = mapperPath.substr(0, pos + 1);
			}
			STARTUPINFOA sui = { sizeof(sui) };
			PROCESS_INFORMATION pii = { 0 };
			DWORD dwFlags = 0;
			HANDLE hOutR = NULL, hOutW = NULL, hErrR = NULL, hErrW = NULL;

			/* Aether.Mapper (type 1): run with visible console so user sees crash/error (Native AOT can exit before writing to pipes). */
			const bool useVisibleConsole = (mapperType == 1);
			if (useVisibleConsole) {
				sui.dwFlags = STARTF_USESHOWWINDOW;
				sui.wShowWindow = SW_SHOW;
				dwFlags = CREATE_NEW_CONSOLE;
			} else {
				SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
				CreatePipe(&hOutR, &hOutW, &sa, 0);
				CreatePipe(&hErrR, &hErrW, &sa, 0);
				SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);
				SetHandleInformation(hErrR, HANDLE_FLAG_INHERIT, 0);
				sui.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
				sui.wShowWindow = SW_HIDE;
				sui.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
				sui.hStdOutput = hOutW;
				sui.hStdError = hErrW;
				dwFlags = CREATE_NO_WINDOW;
			}

			if (!CreateProcessA(mapperPath.c_str(), cmdBuf.data(), NULL, NULL, TRUE, dwFlags, NULL, mapperCwd.c_str(), &sui, &pii)) {
				if (hOutR) { CloseHandle(hOutR); CloseHandle(hOutW); CloseHandle(hErrR); CloseHandle(hErrW); }
				return false;
			}
			if (hOutW) { CloseHandle(hOutW); CloseHandle(hErrW); }
			if (hOutR) {
				char readBuf[512];
				DWORD n = 0;
				for (;;) {
					n = 0;
					if (PeekNamedPipe(hOutR, NULL, 0, NULL, &n, NULL) && n > 0 && ReadFile(hOutR, readBuf, sizeof(readBuf) - 1, &n, NULL) && n > 0) { readBuf[n] = '\0'; mapperOutput += readBuf; }
					n = 0;
					if (PeekNamedPipe(hErrR, NULL, 0, NULL, &n, NULL) && n > 0 && ReadFile(hErrR, readBuf, sizeof(readBuf) - 1, &n, NULL) && n > 0) { readBuf[n] = '\0'; mapperOutput += readBuf; }
					if (WaitForSingleObject(pii.hProcess, 200) == WAIT_OBJECT_0) break;
				}
				for (int d = 0; d < 20; d++) {
					n = 0;
					if (ReadFile(hOutR, readBuf, sizeof(readBuf) - 1, &n, NULL) && n > 0) { readBuf[n] = '\0'; mapperOutput += readBuf; }
					else if (ReadFile(hErrR, readBuf, sizeof(readBuf) - 1, &n, NULL) && n > 0) { readBuf[n] = '\0'; mapperOutput += readBuf; }
					else break;
				}
				CloseHandle(hOutR); CloseHandle(hErrR);
			} else {
				WaitForSingleObject(pii.hProcess, INFINITE);
			}
			DWORD exitCode = 0;
			GetExitCodeProcess(pii.hProcess, &exitCode);
			result = (int)exitCode;
			CloseHandle(pii.hProcess); CloseHandle(pii.hThread);
			return true;
		};

		std::string cmdLine;
		if (mapperType == 0) {
			cmdLine = "\"" + mapperPath + "\" --driver " + std::string(OBF_STR("intel").c_str()) + " \"" + driverPath + "\"";
			if (runMapperWithCmd(cmdLine)) {
				bool collision = (mapperOutput.find("0xc0000033") != std::string::npos) || (mapperOutput.find("Failed to register") != std::string::npos);
				if (result != 0 && collision) {
					printf("[DRIVER] Intel backend failed (0xC0000033). Retrying with eneio backend.\n");
					cmdLine = "\"" + mapperPath + "\" --driver " + std::string(OBF_STR("eneio").c_str()) + " \"" + driverPath + "\"";
					if (runMapperWithCmd(cmdLine) && result == 0)
						printf("[DRIVER] Eneio backend succeeded.\n");
				}
			} else {
				printf("[DRIVER] CreateProcess failed: %lu\n", GetLastError());
			}
		} else if (mapperType == 1) {
			cmdLine = "\"" + mapperPath + "\" -f \"" + driverPath + "\"";
			printf("[DRIVER] Starting Aether.Mapper (visible console). If it closes immediately, check %%TEMP%%\\aether_mapper_trace.txt to see if Main was reached.\n");
			runMapperWithCmd(cmdLine);
		} else {
			/* rtcore (2), legitmemory (3): typical single-arg driver path */
			cmdLine = "\"" + mapperPath + "\" \"" + driverPath + "\"";
			if (!runMapperWithCmd(cmdLine)) {
				DWORD err = GetLastError();
				printf("[DRIVER] CreateProcess failed: %lu\n", err);
			}
		}
	}

	printf("[DRIVER] Mapper result: %d\n", result);
	if (result != 0 && !mapperOutput.empty()) {
		/* Sanitize: kdmapper uses wcout so pipe may contain UTF-16; keep printable ASCII only to avoid crash/garbage */
		std::string safe;
		safe.reserve(mapperOutput.size() > 2048 ? 2048 : mapperOutput.size());
		for (size_t i = 0; i < mapperOutput.size() && safe.size() < 2048; i++) {
			unsigned char c = (unsigned char)mapperOutput[i];
			if (c >= 32 && c < 127) safe += (char)c;
			else if (c == '\n' || c == '\r' || c == '\t') safe += (char)c;
		}
		if (!safe.empty())
			printf("[DRIVER] Mapper output:\n%s\n", safe.c_str());
		bool collision = (safe.find("0xc0000033") != std::string::npos) || (safe.find("Failed to register") != std::string::npos);
		printf("[DRIVER] Hint: ");
		if (collision) {
			printf("Intel backend uses \\Device\\Nal (0xC0000033 when in use). Loader retries with eneio backend; for eneio place eneio64.sys next to kdmapper.exe. Or uninstall Intel Network Adapter/PROSet and reboot. ");
		}
		printf("\n");
	}
	Sleep(3500);  /* Allow driver init + section creation to complete before find_driver */
	
	/* SKIP_VERIFY_OPEN: 1=skip open_hooked_device in LoadDriver. Test: if crash stops, CreateFile was trigger. */
#ifndef SKIP_VERIFY_OPEN
#define SKIP_VERIFY_OPEN 0
#endif
#if !SKIP_VERIFY_OPEN
	/* FlushComm: verify hooked device reachable (open same device driver hooked) */
	HANDLE verify_handle = open_hooked_device(read_hooked_device_index());
	if (verify_handle != INVALID_HANDLE_VALUE) {
		printf("[DRIVER] FlushComm device available. Verifying communication...\n");
		CloseHandle(verify_handle);
	} else if (result != 0) {
		DWORD err = GetLastError();
		printf("[DRIVER] Beep/Null open failed: %lu (0x%lX)\n", err, err);
	}
#endif
	
	if (mapperPath.find("mapper_temp") != std::string::npos) {
		std::remove(mapperPath.c_str());
	}
	
	printf("[DRIVER] Driver loading completed\n");
}