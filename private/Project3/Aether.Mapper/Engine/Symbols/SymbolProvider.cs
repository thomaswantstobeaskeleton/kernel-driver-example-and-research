using Aether.Mapper.Core;
using Aether.Mapper.Core.Logging;
using Aether.Mapper.Interop;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Aether.Mapper.Engine.Symbols;

internal interface ISymbolProvider
{
    Task<int> GetOffsetAsync(string moduleName, string symbolName, CancellationToken cancellationToken);
}

internal sealed class SymbolProvider : ISymbolProvider, IDisposable, IAsyncDisposable
{
    private const int MaxSymName = 2000;

    private readonly Dictionary<string, SymbolModuleInfo> _modules = new(StringComparer.OrdinalIgnoreCase);

    private readonly nint _processHandle;
    private readonly nint _buffer;

    private ulong _baseAddress = 0x40000;

    public SymbolProvider()
    {
        _processHandle = Process.GetCurrentProcess().Handle;

        Ensure.That(DbgHelp.SymInitialize(_processHandle, null, false));

        _buffer = Marshal.AllocHGlobal(Marshal.SizeOf<SymbolInfo>());
    }

    public async Task<int> GetOffsetAsync(string moduleName, string symbolName, CancellationToken cancellationToken)
    {
        var moduleInfo = await LoadModuleAsync(moduleName, cancellationToken);

        var symbolInfo = new SymbolInfo
        {
            SizeOfStruct = (uint)Marshal.SizeOf<SymbolInfo>(),
            MaxNameLen = MaxSymName
        };

        Marshal.StructureToPtr(symbolInfo, _buffer, false);

        Ensure.That(DbgHelp.SymFromName(_processHandle, symbolName, _buffer));

        symbolInfo = Marshal.PtrToStructure<SymbolInfo>(_buffer);

        return (int)(symbolInfo.Address - moduleInfo.ModuleBase);
    }

    private async Task<SymbolModuleInfo> LoadModuleAsync(string moduleName, CancellationToken cancellationToken)
    {
        if (!_modules.TryGetValue(moduleName, out var moduleInfo))
        {
            var pdbPath = await SymbolLoader.LoadAsync(moduleName);
            var pdbSize = (uint)new FileInfo(pdbPath).Length;
            var pdbBase = _baseAddress;
            var moduleBase = DbgHelp.SymLoadModuleEx(_processHandle, nint.Zero, pdbPath, null, pdbBase, pdbSize, nint.Zero, 0);

            Ensure.That(moduleBase != 0, () => $"Failed to load module: {moduleName} (0x{Marshal.GetLastWin32Error():x})");

            moduleInfo = new SymbolModuleInfo(moduleName, moduleBase);

            _modules.Add(moduleName, moduleInfo);
            _baseAddress = Memory.AlignToPageSize(_baseAddress + pdbSize);

            await Log.MessageAsync($"[SYM] Loaded module: {moduleName} at 0x{moduleBase:x} size of {pdbSize} ({pdbPath})", cancellationToken);
        }

        return moduleInfo;
    }

    public void Dispose()
    {
        foreach (var moduleInfo in _modules.Values)
        {
            DbgHelp.SymUnloadModule64(_processHandle, moduleInfo.ModuleBase);
        }

        _modules.Clear();

        Marshal.FreeHGlobal(_buffer);
    }

    public ValueTask DisposeAsync()
    {
        Dispose();
        return ValueTask.CompletedTask;
    }

    private sealed record SymbolModuleInfo(string Name, ulong ModuleBase);
}