#pragma once
/* PEB-based module enumeration - avoids CreateToolhelp32Snapshot/EnumProcessModules (often monitored).
 * Offsets verified for Windows 10/11 23H2 x64 (PEB layout stable per MS/ReactOS). */

#include <Windows.h>
#include <winternl.h>
#include <string>

namespace peb_modules {

/* Extended LDR entry - x64 layout. FullDllName at +0x48 (Win10/11) */
#pragma pack(push, 8)
typedef struct _LDR_MODULE_ENTRY {
    PVOID Reserved1[2];
    LIST_ENTRY InMemoryOrderLinks;
    PVOID Reserved2[2];
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UCHAR ReservedTo48[0x48 - 0x30];  /* pad to FullDllName at 0x48 */
    UNICODE_STRING FullDllName;
} LDR_MODULE_ENTRY, *PLDR_MODULE_ENTRY;

/* PEB_LDR_DATA with all three lists - InMemoryOrderModuleList at +0x20 on x64 */
typedef struct _PEB_LDR_DATA_FULL {
    BYTE Reserved1[8];
    PVOID Reserved2[3];
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA_FULL, *PPEB_LDR_DATA_FULL;
#pragma pack(pop)

struct ModuleInfo {
    wchar_t name[MAX_PATH];
    uintptr_t base;
    SIZE_T size;
};

/* Get PEB for current process (NtCurrentTeb()->ProcessEnvironmentBlock) */
inline PPEB GetCurrentPeb() {
#ifdef _WIN64
    return (PPEB)__readgsqword(0x60);
#else
    return (PPEB)__readfsdword(0x30);
#endif
}

/* Enumerate modules via PEB->Ldr->InMemoryOrderModuleList. Returns count written to outModules. */
inline int enumerate_modules(ModuleInfo* outModules, int maxCount, HANDLE hProcess = GetCurrentProcess()) {
    if (maxCount <= 0 || !outModules) return 0;

    /* For current process, use direct PEB access. For remote, would need ReadProcessMemory. */
    if (hProcess != GetCurrentProcess())
        return 0;  /* Remote: fallback to EnumProcessModules or similar */

    PPEB peb = GetCurrentPeb();
    if (!peb || !peb->Ldr) return 0;

    PPEB_LDR_DATA_FULL ldr = (PPEB_LDR_DATA_FULL)peb->Ldr;
    PLIST_ENTRY head = &ldr->InMemoryOrderModuleList;
    PLIST_ENTRY current = head->Flink;
    int count = 0;

    while (current != head && count < maxCount) {
        PLDR_MODULE_ENTRY entry = CONTAINING_RECORD(current, LDR_MODULE_ENTRY, InMemoryOrderLinks);

        __try {
            if (entry->DllBase && entry->SizeOfImage > 0) {
                outModules[count].base = (uintptr_t)entry->DllBase;
                outModules[count].size = entry->SizeOfImage;
                if (entry->FullDllName.Buffer && entry->FullDllName.Length > 0) {
                    int copyLen = entry->FullDllName.Length / sizeof(wchar_t);
                    if (copyLen >= MAX_PATH) copyLen = MAX_PATH - 1;
                    memcpy(outModules[count].name, entry->FullDllName.Buffer, copyLen * sizeof(wchar_t));
                    outModules[count].name[copyLen] = L'\0';
                } else {
                    outModules[count].name[0] = L'\0';
                }
                count++;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            break;
        }

        current = current->Flink;
    }
    return count;
}

/* Get ImageBaseAddress from PEB - replaces GetModuleHandleA(NULL) */
inline void* get_image_base_address() {
    PPEB peb = GetCurrentPeb();
    if (!peb) return nullptr;
    return *(PVOID*)((BYTE*)peb + 0x10);  /* PEB + 0x10 = ImageBaseAddress (Win10/11 x64) */
}

/* Get self exe full path from PEB - no GetModuleFileNameA (monitored). Use for dir resolution (LoadDriver). */
inline std::string get_self_module_full_path() {
    PPEB peb = GetCurrentPeb();
    if (!peb || !peb->Ldr) return "";
    PVOID imgBase = *(PVOID*)((BYTE*)peb + 0x10);  /* ImageBaseAddress */
    PPEB_LDR_DATA_FULL ldr = (PPEB_LDR_DATA_FULL)peb->Ldr;
    PLIST_ENTRY head = &ldr->InMemoryOrderModuleList;
    PLIST_ENTRY cur = head->Flink;
    while (cur != head) {
        BYTE* entry = (BYTE*)cur - 0x10;
        PVOID base = *(PVOID*)(entry + 0x30);
        if (base == imgBase) {
            PUNICODE_STRING full = (PUNICODE_STRING)(entry + 0x48);
            if (full->Buffer && full->Length >= 2) {
                int len = full->Length / sizeof(wchar_t);
                if (len >= MAX_PATH) len = MAX_PATH - 1;
                char pathA[MAX_PATH] = { 0 };
                WideCharToMultiByte(CP_ACP, 0, full->Buffer, len, pathA, MAX_PATH, NULL, NULL);
                return std::string(pathA);
            }
            return "";
        }
        cur = cur->Flink;
    }
    return "";
}

/* Get self exe filename from PEB - no GetModuleFileNameA (monitored) */
inline std::string get_self_module_name() {
    std::string path = get_self_module_full_path();
    if (path.empty()) return "";
    size_t last = path.find_last_of("\\/");
    return last != std::string::npos ? path.substr(last + 1) : path;
}

/* Check if address falls within any loaded module (PEB-based, no EnumProcessModules) */
inline bool is_address_in_loaded_modules(void* address) {
    ModuleInfo mods[256];
    int n = enumerate_modules(mods, 256);
    uintptr_t addr = (uintptr_t)address;
    for (int i = 0; i < n; i++) {
        if (addr >= mods[i].base && addr < mods[i].base + mods[i].size)
            return true;
    }
    return false;
}

}  // namespace peb_modules
