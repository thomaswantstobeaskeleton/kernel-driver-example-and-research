using Aether.Mapper.Core;
using Aether.Mapper.Runtime;
using System.Runtime.InteropServices;
using Aether.Mapper.Core.Logging;
using Aether.Mapper.Interop;
using Aether.Mapper.Engine.Symbols;

namespace Aether.Mapper.Engine.TraceCleaners;

internal sealed class PiDDBCleaner(DriverInterface driver, NtoskrnlModule module, ISymbolProvider symbolProvider) : ITraceCleaner
{
    private const string ModuleName = "ntoskrnl.exe";

    public async Task CleanAsync(CancellationToken cancellationToken)
    {
        var dbLock = module.BaseAddress + await symbolProvider.GetOffsetAsync(ModuleName, "PiDDBLock", cancellationToken);
        var dbCacheTable = module.BaseAddress + await symbolProvider.GetOffsetAsync(ModuleName, "PiDDBCacheTable", cancellationToken);

        Ensure.That(dbLock != nint.Zero, () => "Failed to locate: PiDDBLock");
        Ensure.That(dbCacheTable != nint.Zero, () => "Failed to locate: PiDDBCacheTable");

        using var _ = module.AcquireResourceExclusiveLite(dbLock);

        var foundEntry = LookupElementGenericTableAvl(dbCacheTable);

        await Log.MessageAsync($"[PiDDB] Found entry: 0x{foundEntry:x}", cancellationToken);

        var blinkOffset = Marshal.OffsetOf<ListEntry>(nameof(ListEntry.Blink));
        var flinkOffset = Marshal.OffsetOf<ListEntry>(nameof(ListEntry.Flink));

        var blink = driver.Read<nint>(foundEntry + blinkOffset);
        var flink = driver.Read<nint>(foundEntry + flinkOffset);

        driver.Write(blink + flinkOffset, flink);
        driver.Write(flink + blinkOffset, blink);

        await Log.MessageAsync($"[PiDDB] Entry unlinked: 0x{foundEntry:x}", cancellationToken);

        module.DeleteElementGenericTableAvl(dbCacheTable, foundEntry);

        await Log.MessageAsync($"[PiDDB] Entry deleted: 0x{foundEntry:x}", cancellationToken);

        var table = driver.Read<RtlAvlTable>(dbCacheTable);

        driver.Write(dbCacheTable + Marshal.OffsetOf<RtlAvlTable>(nameof(RtlAvlTable.DeleteCount)), table.DeleteCount - 1);

        await Log.MessageAsync($"[PiDDB] Table delete count decremented: {table.DeleteCount} -> {table.DeleteCount - 1}", cancellationToken);
        await Log.MessageAsync($"[PiDDB] Traces successfully cleaned", cancellationToken);
    }

    private nint LookupElementGenericTableAvl(nint dbCacheTable)
    {
        using var driverName = UnicodeString.Create($"{driver.DriverName}.sys");

        var dbCacheEntry = new PiDDBCacheEntry
        {
            DriverName = driverName,
            TimeDateStamp = driver.Timestamp
        };

        var dbCacheEntryAddress = Marshal.AllocHGlobal(Marshal.SizeOf(dbCacheEntry));

        try
        {
            Marshal.StructureToPtr(dbCacheEntry, dbCacheEntryAddress, false);
            return module.LookupElementGenericTableAvl(dbCacheTable, dbCacheEntryAddress);
        }
        finally
        {
            Marshal.FreeHGlobal(dbCacheEntryAddress);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct RtlBalancedLinks
    {
        public nint Parent; 
        public nint LeftChild;
        public nint RightChild;
        public byte Balance;
        public byte Reserved1;
        public ushort Reserved2;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct RtlAvlTable
    {
        public RtlBalancedLinks BalancedRoot;
        public nint OrderedPointer;
        public uint WhichOrderedElement;
        public uint NumberGenericTableElements;
        public uint DepthOfTree;
        public nint RestartKey;
        public uint DeleteCount;
        public nint CompareRoutine;
        public nint AllocateRoutine;
        public nint FreeRoutine;
        public nint TableContext;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PiDDBCacheEntry
    {
        public ListEntry List;
        public UnicodeString DriverName;
        public uint TimeDateStamp;
        public NtStatus LoadStatus;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public byte[] ShimData;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ListEntry
    {
        public nint Flink;
        public nint Blink;
    }
}