using Aether.Mapper.Engine;
using Aether.Mapper.Engine.Symbols;
using Aether.Mapper.Engine.TraceCleaners;
using Aether.Mapper.Runtime;
using Jab;

namespace Aether.Mapper;

[ServiceProvider]
[Singleton<DriverInterface>(Instance = nameof(Driver))]
[Singleton<NtoskrnlModule>(Instance = nameof(NtoskrnlModule))]
[Singleton<ITraceCleaner, PiDDBCleaner>]
[Singleton<ITraceCleaner, KernelHashBucketListCleaner>]
[Singleton<ITraceCleaner, MmUnloadedDriversCleaner>]
[Singleton<ITraceCleaner, WdFilterCleaner>]
[Singleton<ISymbolProvider, SymbolProvider>]
[Singleton<MemoryProvider>]
internal partial class AppServiceProvider
{
    public required DriverInterface Driver { get; set; }
    public required NtoskrnlModule NtoskrnlModule { get; set; }
}