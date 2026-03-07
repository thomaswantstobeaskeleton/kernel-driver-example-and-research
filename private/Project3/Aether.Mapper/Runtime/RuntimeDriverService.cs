using Aether.Mapper.Core;
using Aether.Mapper.Interop;
using Aether.Mapper.Extensions;
using Microsoft.Win32;

using RegistryHive = Microsoft.Win32.RegistryHive;
using RegistryKey = Microsoft.Win32.RegistryKey;

namespace Aether.Mapper.Runtime;

internal sealed class RuntimeDriverService : IDisposable
{
    private const string ServicesKeyPath = @"System\CurrentControlSet\Services";

    private readonly string _serviceName;
    private readonly string _registryPath;

    private RuntimeDriverService(string serviceName, string registryPath)
    {
        _serviceName = serviceName;
        _registryPath = @$"\Registry\Machine\{ServicesKeyPath}\{serviceName}";
    }

    public unsafe void Dispose()
    {
        try
        {
            using var driverDeviceName = UnicodeString.Create(_registryPath);
            Ntdll.NtUnloadDriver(&driverDeviceName);
        }
        finally
        {
            DeleteKeyTree(_serviceName);
        }
    }

    public static RuntimeDriverService CreateNoLoad(string serviceName, string servicePath)
    {
        using var machineKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
        using var servicesKey = machineKey.OpenRequiredKey(ServicesKeyPath);
        using var serviceKey = servicesKey.CreateSubKey(serviceName);

        serviceKey.SetValue("ImagePath", @$"\??\{servicePath}", RegistryValueKind.ExpandString);
        serviceKey.SetValue("Type", 1, RegistryValueKind.DWord);

        try
        {
            var registryPath = @$"\Registry\Machine\{ServicesKeyPath}\{serviceName}";

            return new RuntimeDriverService(serviceName, registryPath);
        }
        catch
        {
            DeleteKeyTree(serviceName);
            throw;
        }
    }

    public static unsafe RuntimeDriverService CreateAndLoad(string serviceName, string servicePath)
    {
        using var machineKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
        using var servicesKey = machineKey.OpenRequiredKey(ServicesKeyPath);
        using var serviceKey = servicesKey.CreateSubKey(serviceName);

        serviceKey.SetValue("ImagePath", @$"\??\{servicePath}", RegistryValueKind.ExpandString);
        serviceKey.SetValue("Type", 1, RegistryValueKind.DWord);

        try
        {
            var registryPath = @$"\Registry\Machine\{ServicesKeyPath}\{serviceName}";
            using var driverDeviceName = UnicodeString.Create(registryPath);

            Ensure.ThatSuccess(Ntdll.NtLoadDriver(&driverDeviceName));

            return new RuntimeDriverService(serviceName, registryPath);
        }
        catch
        {
            DeleteKeyTree(serviceName);
            throw;
        }
    }

    private static void DeleteKeyTree(string serviceName)
    {
        using var machineKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
        using var servicesKey = machineKey.OpenSubKey(ServicesKeyPath, writable: true);

        servicesKey?.DeleteSubKeyTree(serviceName);
    }
}