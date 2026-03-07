using System.Text;
using Aether.Mapper.Core;
using Aether.Mapper.Core.Logging;
using Aether.Mapper.Interop;
using Aether.Mapper.Runtime;

namespace Aether.Mapper.Engine.TraceCleaners;

internal sealed class MmUnloadedDriversCleaner(DriverInterface driver) : ITraceCleaner
{
    public async Task CleanAsync(CancellationToken cancellationToken)
    {
        var objectAddress = SystemInformation.GetObjectByDeviceHandle(driver.DeviceHandle);
        var deviceObject = Read(objectAddress, 0x8, "DeviceObject");
        var driverObject = Read(deviceObject, 0x8, "DriverObject");
        var driverSection = Read(driverObject, 0x28, "DriverSection");

        var baseDllNameString = driver.Read<UnicodeString>(driverSection + 0x58);
        var baseDllName = driver.Read(baseDllNameString.Buffer, Encoding.Unicode, baseDllNameString.Length << 1);

        baseDllNameString.Length = 0;

        driver.Write(driverSection + 0x58, baseDllNameString);

        await Log.MessageAsync($"[MmUnloadedDrivers] Driver name length zeroed: {baseDllName}", cancellationToken);
        await Log.MessageAsync("[MmUnloadedDrivers] Traces successfully cleaned", cancellationToken);
    }

    private nint Read(nint address, int offset, string name)
    {
        var pointer = driver.Read<nint>(address + offset);

        Ensure.That(pointer != nint.Zero, () => $"[MmUnloadedDrivers] {name} not found");

        return pointer;
    }
}