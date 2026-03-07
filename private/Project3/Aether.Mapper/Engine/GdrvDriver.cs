using System.Runtime.InteropServices;
using Aether.Mapper.Core;
using Aether.Mapper.PE32;
using Aether.Mapper.Properties;
using Aether.Mapper.Runtime;

namespace Aether.Mapper.Engine;

internal sealed unsafe class GdrvDriver : DriverInterface, IDisposable
{
    private const string DeviceName = @"\Device\GIO";
    private const int XorKey = 0x571d77a5;

    private readonly RuntimeDriver _driver;
    private readonly PhysicalAddressProvider _physicalAddressProvider = new();

    private GdrvDriver(RuntimeDriver driver, uint timestamp)
    {
        _driver = driver;
        Timestamp = timestamp;
    }

    public override uint Timestamp { get; }
    public override string DriverName => _driver.DriverName;
    public override string DriverPath => _driver.DriverPath;
    public override nint DeviceHandle => _driver.Device.DeviceHandle;

    protected override void CopyMemory(nint sourceAddress, nint targetAddress, int length)
    {
        var request = new MemoryCopyRequest(targetAddress, sourceAddress, length);

        _driver.Device.Request(0xc3502808, &request);
    }

    public override nint MapIoSpace(nint physicalAddress, int size)
    {
        var request = new MemoryMapRequest(physicalAddress, size);
        var result = _driver.Device.Request<MemoryMapRequest, nint>(0xC3502004, &request);

        return result;
    }

    public override void UnmapIoSpace(nint address)
    {
        _driver.Device.Request(0xC3502008, &address);
    }

    public override PhysicalAddressInfo GetPhysicalAddress(nint address)
    {
        return _physicalAddressProvider.GetPhysicalAddress(this, address);
    }

    public void Dispose()
    {
        _driver.Dispose();
    }

    public static GdrvDriver Load()
    {
        var data = GetDriverData();
        var pe = PortableExecutable.Load(data);
        var timestamp = pe.FileHeader.TimeDateStamp;
        var driver = RuntimeDriver.Load(DeviceName, data);

        return new GdrvDriver(driver, timestamp);
    }

    private static byte[] GetDriverData()
    {
        return Security.Xor(Resources.Binary, XorKey);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MemoryCopyRequest(nint dst, nint src, int size)
    {
        public nint Dst = dst;
        public nint Src = src;
        public int Size = size;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MemoryMapRequest(nint address, int size)
    {
        public uint InterfaceType;
        public uint BusNumber;
        public nint Address = address;
        public uint AddressSpace;
        public int Size = size;
    }
}