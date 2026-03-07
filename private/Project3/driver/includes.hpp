#pragma once
#include <ntifs.h>
#include <windef.h>

/* PE DOS magic derived from chars so no literal 0x5A4D in source/binary as single constant signature. */
#ifndef FLUSHCOMM_PE_MAGIC_MZ
#define FLUSHCOMM_PE_MAGIC_MZ  ((USHORT)((('M' << 8) | 'Z')))
#endif

//global offsets - declared here, defined in driver.cpp
namespace globals
{
    extern ULONG u_ver_build;
    extern ULONG u_ver_major;

    namespace offsets
    {
        extern int i_image_file_name;
        extern int i_active_threads;
        extern int i_active_process_links;
        extern int i_peb;
        extern int i_user_dirbase;
        extern int i_unique_process_id;
    }


    //undefined kernel structs
    typedef struct _LDR_DATA_TABLE_ENTRY
    {
        LIST_ENTRY InLoadOrderLinks;
        LIST_ENTRY InMemoryOrderLinks;
        LIST_ENTRY InInitializationOrderLinks;
        PVOID DllBase;
        PVOID EntryPoint;
        ULONG SizeOfImage;
        UNICODE_STRING FullDllName;
        UNICODE_STRING BaseDllName;
        ULONG Flags;
        WORD LoadCount;
        WORD TlsIndex;
        union
        {
            LIST_ENTRY HashLinks;
            struct
            {
                PVOID SectionPointer;
                ULONG CheckSum;
            };
        };
        union
        {
            ULONG TimeDateStamp;
            PVOID LoadedImports;
        };
        VOID* EntryPointActivationContext;
        PVOID PatchInformation;
        LIST_ENTRY ForwarderLinks;
        LIST_ENTRY ServiceTagLinks;
        LIST_ENTRY StaticLinks;
    } LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;


    //undefined kernel functions
    extern "C"
    {
        __declspec(dllimport) PLIST_ENTRY NTAPI PsLoadedModuleList;
        extern POBJECT_TYPE* IoDriverObjectType;  /* from ntoskrnl.lib - no dllimport to avoid LNK2005 */
        __declspec(dllimport) PVOID NTAPI RtlFindExportedRoutineByName( PVOID, PCCH );
        __declspec(dllimport) PVOID NTAPI PsGetProcessSectionBaseAddress( PEPROCESS );
        __declspec(dllimport) NTSTATUS NTAPI ZwProtectVirtualMemory( HANDLE, PVOID*, PSIZE_T, ULONG, PULONG );
        __declspec(dllimport) PIMAGE_NT_HEADERS NTAPI RtlImageNtHeader( PVOID );
    }
}