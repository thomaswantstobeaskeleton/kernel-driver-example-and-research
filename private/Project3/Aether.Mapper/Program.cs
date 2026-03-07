using Aether.Mapper;
using Aether.Mapper.Core;
using Aether.Mapper.Core.Logging;
using Aether.Mapper.Engine;
using Aether.Mapper.Interop;
using Aether.Mapper.Runtime;
using Microsoft.Win32;

internal static class Program
{
    private static async Task Main(string[] args)
    {
        using var cts = new CancellationTokenSource();

        try
        {
            var options = Options.Parse(args);

            if (options.LogConsole)
                Log.AddTransport(new ConsoleLoggerTransport());

            if (!string.IsNullOrEmpty(options.LogFile))
                Log.AddTransport(new FileLoggerTransport(options.LogFile));

            Console.CancelKeyPress += (sender, e) =>
            {
                e.Cancel = true;
                cts.Cancel();
            };

            CleanPrevious();

            using var driver = GdrvDriver.Load();
            await using var provider = new AppServiceProvider()
            {
                Driver = driver,
                NtoskrnlModule = NtoskrnlModule.Load(driver)
            };

            var cleaners = provider.GetService<IEnumerable<ITraceCleaner>>();

            foreach (var cleaner in cleaners)
                await cleaner.CleanAsync(cts.Token);

            await provider.GetService<MemoryProvider>().GetMemoryAsync(Memory.PageSize * 4, cts.Token);

            //var mapper = new Mapper(driver, ntoskrnlModule);
            //mapper.Map(options.FilePath);
        }
        catch (Exception e)
        {
            await Log.ExceptionAsync(e, cts.Token);
        }
        finally
        {
            await CleanPrefetchAsync(cts.Token);
            await Log.MessageAsync("Completed", cts.Token);
        }
    }

    private static unsafe void CleanPrevious()
    {
        const string serviceKeyPath = @"SYSTEM\CurrentControlSet\Services";

        var directoryPath = Environment.GetEnvironmentVariable("TEMP")!;
        var files = Directory.GetFiles(directoryPath, "*.sys", SearchOption.TopDirectoryOnly);

        if (files.Length > 0)
            Ensure.ThatSuccess(Ntdll.RtlAdjustPrivilege(Privilege.LoadDriver, true, false, out _));

        foreach (var filePath in files)
        {
            var serviceName = Path.GetFileNameWithoutExtension(filePath);

            using var driverDeviceName = UnicodeString.Create(@$"\Registry\Machine\{serviceKeyPath}\{serviceName}");
            var status = Ntdll.NtUnloadDriver(&driverDeviceName);

            using var machineKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
            using var servicesKey = machineKey.OpenSubKey(serviceKeyPath, writable: true);

            servicesKey?.DeleteSubKeyTree(serviceName);

            File.Delete(Path.Combine(Environment.GetEnvironmentVariable("TEMP")!, $"{serviceName}.sys"));
        }
    }

    private static async Task CleanPrefetchAsync(CancellationToken cancellationToken)
    {
        try
        {
            var windowsDirectoryPath = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
            var prefetchDirectoryPath = Path.Combine(windowsDirectoryPath, "Prefetch");
            var processName = Path.GetFileName(Environment.ProcessPath);

            Ensure.That(!string.IsNullOrEmpty(processName));

            var files = Directory.GetFiles(prefetchDirectoryPath, $"{processName}*.pf");

            foreach (var filePath in files)
            {
                try
                {
                    File.Delete(filePath);
                    await Log.MessageAsync($"[Prefetch] Removed file: {filePath}", cancellationToken);
                }
                catch (Exception e)
                {
                    await Log.MessageAsync($"[Prefetch] Failed to remove: {filePath}", cancellationToken);
                }
            }
        }
        catch
        {
            // ignore
        }
    }
}