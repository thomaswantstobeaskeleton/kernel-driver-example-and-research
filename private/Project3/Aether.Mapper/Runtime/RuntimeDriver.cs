using Aether.Mapper.Core;
using Aether.Mapper.Interop;

namespace Aether.Mapper.Runtime;

internal sealed class RuntimeDriver : IDisposable
{
    private readonly Device _device;
    private readonly IDisposable _context;

    private RuntimeDriver(Device device, IDisposable context, string driverName, string driverPath)
    {
        _device = device;
        _context = context;

        DriverName = driverName;
        DriverPath = driverPath;
    }

    public string DriverName { get; }
    public string DriverPath { get; }
    public IDevice Device => _device;

    public void Dispose()
    {
        _device.Dispose();
        _context.Dispose();
    }

    public static RuntimeDriver Load(string deviceName, byte[] data)
    {
        Ensure.ThatSuccess(Ntdll.RtlAdjustPrivilege(Privilege.LoadDriver, true, false, out _));
        Ensure.That(!Runtime.Device.Exists(deviceName), () => $"Device is already in use: {deviceName}");

        var driverName = Rand.GenerateName(8);
        var context = new Disposables();

        try
        {
            var file = context.Add(TemporaryFile.Create(data, $"{driverName}.sys"));

            context.Add(RuntimeDriverService.CreateAndLoad(driverName, file.Path));

            var device = Runtime.Device.Open(deviceName);

            return new RuntimeDriver(device, context, driverName, file.Path);
        }
        catch (Exception)
        {
            context.Dispose();
            throw;
        }
    }
}