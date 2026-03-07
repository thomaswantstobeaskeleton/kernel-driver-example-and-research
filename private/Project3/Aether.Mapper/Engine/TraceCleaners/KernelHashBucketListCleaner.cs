using System.Runtime.InteropServices;
using System.Text;
using Aether.Mapper.Core.Logging;
using Aether.Mapper.Engine.Symbols;
using Aether.Mapper.Interop;
using Aether.Mapper.Runtime;

namespace Aether.Mapper.Engine.TraceCleaners;

internal sealed class KernelHashBucketListCleaner(DriverInterface driver, NtoskrnlModule module, ISymbolProvider symbolProvider) : ITraceCleaner
{
    private const string CiModuleName = "ci.dll";

    public async Task CleanAsync(CancellationToken cancellationToken)
    {
        var ciModule = KernelModule.Load(driver, CiModuleName);
        var list = ciModule.BaseAddress + await symbolProvider.GetOffsetAsync(CiModuleName, "g_KernelHashBucketList", cancellationToken);
        var cacheLock = ciModule.BaseAddress + await symbolProvider.GetOffsetAsync(CiModuleName, "g_HashCacheLock", cancellationToken);

        using var _ = module.AcquireResourceExclusiveLite(cacheLock);

        var driverPath = driver.DriverPath[driver.DriverPath.IndexOf('\\')..];
        var driverPathLength = Encoding.Unicode.GetByteCount(driverPath);
        var nextOffset = Marshal.OffsetOf<HashBucketEntry>(nameof(HashBucketEntry.Next));

        var prev = list;
        var entry = driver.Read<nint>(list + nextOffset);

        while (entry != nint.Zero)
        {
            var unicodeString = driver.Read<UnicodeString>(entry + Marshal.OffsetOf<HashBucketEntry>(nameof(HashBucketEntry.DriverName)));

            if (unicodeString.Length == driverPathLength)
            {
                var name = driver.Read(unicodeString.Buffer, Encoding.Unicode, unicodeString.Length);

                if (driverPath.Equals(name, StringComparison.OrdinalIgnoreCase))
                {
                    var next = driver.Read<nint>(entry + nextOffset);

                    driver.Write(prev + nextOffset, next);
                    module.FreePool(entry);

                    await Log.MessageAsync($"[KernelHashBucketList] Entry cleared: {name}", cancellationToken);
                }
            }

            prev = entry;
            entry = driver.Read<nint>(entry + nextOffset);
        }

        await Log.MessageAsync($"[KernelHashBucketList] Traces successfully cleaned", cancellationToken);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct HashBucketEntry
    {
        public nint Next;
        public UnicodeString DriverName;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 5)]
        public uint[] CertHash;
    }
}