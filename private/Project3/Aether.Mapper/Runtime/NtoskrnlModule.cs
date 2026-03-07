using Aether.Mapper.Core;
using Aether.Mapper.Kernel;
using System.Runtime.InteropServices;

namespace Aether.Mapper.Runtime;

internal sealed class NtoskrnlModule
{
    private const string ModuleName = "ntoskrnl.exe";

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate bool ExAcquireResourceExclusiveLite(nint resource, bool wait);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void ExReleaseResourceLite(nint resource);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate nint RtlLookupElementGenericTableAvl(nint table, nint buffer);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate bool RtlDeleteElementGenericTableAvl(nint table, nint buffer);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate nint ExAllocatePool(PoolType poolType, nint size);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void ExFreePool(nint address);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate bool MmIsAddressValid(nint address);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate nint MmGetPhysicalAddress(nint baseAddress);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate nint MmAllocatePagesForMdl(nint lowAddress, nint highAddress, nint skipBytes, int totalBytes);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void MmFreePagesFromMdl(nint mdl);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate nint MmMapLockedPagesSpecifyCache(nint mdl, KProcessorMode accessMode, MemoryCachingType cacheType, nint requestedAddress, [MarshalAs(UnmanagedType.U1)] bool bugCheckOnFailure, MmPagePriority priority);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void IoFreeMdl(nint mdl);

    private readonly KernelModule _module;
    private readonly KernelFunctionCaller _caller;

    private NtoskrnlModule(KernelModule module, KernelFunctionCaller caller)
    {
        _module = module;
        _caller = caller;
    }

    public nint BaseAddress => _module.BaseAddress;

    public nint GetAddress(string sectionName, Signature[] signatures) => _module.GetAddress(sectionName, signatures);

    public IDisposable AcquireResourceExclusiveLite(nint address)
    {
        Ensure.That(Call<bool, ExAcquireResourceExclusiveLite>(address, true), () => $"Failed to acquire lock for resource: 0x{address:x}");
        return new AutoResource(() => Call<ExReleaseResourceLite>(address));
    }

    public nint LookupElementGenericTableAvl(nint table, nint buffer)
    {
        return Call<nint, RtlLookupElementGenericTableAvl>(table, buffer);
    }

    public void DeleteElementGenericTableAvl(nint table, nint buffer)
    {
        Ensure.That(Call<bool, RtlDeleteElementGenericTableAvl>(table, buffer));
    }

    public nint AllocatePool(PoolType poolType, long size)
    {
        var address = Call<nint, ExAllocatePool>(poolType, new nint(size));
        Ensure.That(address != nint.Zero, () => $"Failed to allocate pool of size: {size}");
        return address;
    }

    public nint GetPhysicalAddress(nint baseAddress)
    {
        return Call<nint, MmGetPhysicalAddress>(baseAddress);
    }

    public bool IsAddressValid(nint address) => Call<bool, MmIsAddressValid>(address);

    public nint AllocatePagesForMdl(nint lowAddress, nint highAddress, nint skipBytes, int totalBytes)
    {
        var address = Call<nint, MmAllocatePagesForMdl>(lowAddress, highAddress, skipBytes, totalBytes);
        Ensure.That(address != nint.Zero, () => $"Failed to allocate pages for mdl of size: {totalBytes} at address: 0x{address:x}");
        return address;
    }

    public void FreePagesFromMdl(nint mdl) => Call<MmFreePagesFromMdl>(mdl);

    public nint MapLockedPagesSpecifyCache(nint mdl, KProcessorMode accessMode, MemoryCachingType cacheType, nint requestedAddress, bool bugCheckOnFailure, MmPagePriority priority)
    {
        var address = Call<nint, MmMapLockedPagesSpecifyCache>(mdl, accessMode, cacheType, requestedAddress, bugCheckOnFailure, priority);
        Ensure.That(address != nint.Zero, () => $"Failed to map locked pages at address: 0x{address:x}");
        return address;
    }

    public void FreeMdl(nint address) => Call<IoFreeMdl>(address);
    public void FreePool(nint address) => Call<ExFreePool>(address);

    public void CallCore<TDelegate>(nint address, params object[] args) where TDelegate : Delegate
    {
        _caller.Call<TDelegate>(address, args);
    }

    public TResult CallCore<TResult, TDelegate>(nint address, params object[] args) where TDelegate : Delegate
    {
        return _caller.Call<TResult, TDelegate>(address, args);
    }

    private TResult Call<TResult, TDelegate>(params object[] args) where TDelegate : Delegate
    {
        var function = _module.PE.GetExportFunction(typeof(TDelegate).Name);
        var address = _module.BaseAddress + function.Address;

        return _caller.Call<TResult, TDelegate>(address, args);
    }

    private void Call<TDelegate>(params object[] args) where TDelegate : Delegate
    {
        var function = _module.PE.GetExportFunction(typeof(TDelegate).Name);
        var address = _module.BaseAddress + function.Address;

        _caller.Call<TDelegate>(address, args);
    }

    public static NtoskrnlModule Load(DriverInterface driver)
    {
        var module = KernelModule.Load(driver, ModuleName);
        var caller = KernelFunctionCaller.Create(driver, module.PE, module.BaseAddress);

        return new NtoskrnlModule(module, caller);
    }
}