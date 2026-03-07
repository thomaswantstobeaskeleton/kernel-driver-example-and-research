using System.Runtime.InteropServices;

namespace Aether.Mapper.PE32;

[StructLayout(LayoutKind.Sequential)]
internal struct ImageLoadConfigDirectory
{
    public uint Size;
    public uint TimeDateStamp;
    public ushort MajorVersion;
    public ushort MinorVersion;
    public uint GlobalFlagsClear;
    public uint GlobalFlagsSet;
    public uint CriticalSectionDefaultTimeout;
    public ulong DeCommitFreeBlockThreshold;
    public ulong DeCommitTotalFreeThreshold;
    public ulong LockPrefixTable;                // VA
    public ulong MaximumAllocationSize;
    public ulong VirtualMemoryThreshold;
    public ulong ProcessAffinityMask;
    public uint ProcessHeapFlags;
    public ushort CSDVersion;
    public ushort DependentLoadFlags;
    public ulong EditList;                       // VA
    public ulong SecurityCookie;                 // VA
    public ulong SEHandlerTable;                 // VA
    public ulong SEHandlerCount;
    public ulong GuardCFCheckFunctionPointer;    // VA
    public ulong GuardCFDispatchFunctionPointer; // VA
    public ulong GuardCFFunctionTable;           // VA
    public ulong GuardCFFunctionCount;
    public uint GuardFlags;
    public ImageLoadConfigCodeIntegrity CodeIntegrity;
    public ulong GuardAddressTakenIatEntryTable; // VA
    public ulong GuardAddressTakenIatEntryCount;
    public ulong GuardLongJumpTargetTable;       // VA
    public ulong GuardLongJumpTargetCount;
    public ulong DynamicValueRelocTable;         // VA
    public ulong CHPEMetadataPointer;            // VA
    public ulong GuardRFFailureRoutine;          // VA
    public ulong GuardRFFailureRoutineFunctionPointer; // VA
    public uint DynamicValueRelocTableOffset;
    public ushort DynamicValueRelocTableSection;
    public ushort Reserved2;
    public ulong GuardRFVerifyStackPointerFunctionPointer; // VA
    public uint HotPatchTableOffset;
    public uint Reserved3;
    public ulong EnclaveConfigurationPointer;    // VA
    public ulong VolatileMetadataPointer;        // VA
    public ulong GuardEHContinuationTable;       // VA
    public ulong GuardEHContinuationCount;
    public ulong GuardXFGCheckFunctionPointer;   // VA
    public ulong GuardXFGDispatchFunctionPointer; // VA
    public ulong GuardXFGTableDispatchFunctionPointer; // VA
    public ulong CastGuardOsDeterminedFailureMode; // VA
    public ulong GuardMemcpyFunctionPointer;     // VA
}

[StructLayout(LayoutKind.Sequential)]
internal struct ImageLoadConfigCodeIntegrity
{
    public ushort Flags;
    public ushort Catalog;
    public uint CatalogOffset;
    public uint Reserved;
}