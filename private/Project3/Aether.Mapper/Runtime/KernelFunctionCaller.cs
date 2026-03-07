using System.Runtime.InteropServices;
using Aether.Mapper.Core;
using Aether.Mapper.Interop;
using Aether.Mapper.PE32;

namespace Aether.Mapper.Runtime;

internal sealed class KernelFunctionCaller
{
    private const int MaxArguments = 6;
    private const string NtdllLibraryName = "ntdll.dll";
    private const string NtAddAtomFunctionName = "NtDeleteAtom";

    private readonly DriverInterface _driver;
    private readonly nint _kernelNtAddAtomAddress;
    private readonly nint _ntAddAtomAddress;

    private KernelFunctionCaller(DriverInterface driver, nint kernelNtAddAtomAddress, nint ntAddAtomAddress)
    {
        _driver = driver;
        _kernelNtAddAtomAddress = kernelNtAddAtomAddress;
        _ntAddAtomAddress = ntAddAtomAddress;
    }

    public void Call<TDelegate>(nint address, params object[] arguments) where TDelegate : Delegate
    {
        Ensure.That(arguments.Length <= MaxArguments, () => $"Too many arguments. Only up to {MaxArguments} are supported.");
        Ensure.That(address != 0, () => "Invalid kernel function address.");

        using var patch = KernelPatch.Patch(_driver, _kernelNtAddAtomAddress, address);
        var function = Marshal.GetDelegateForFunctionPointer<TDelegate>(_ntAddAtomAddress);
        function.DynamicInvoke(arguments);
    }

    public TResult Call<TResult, TDelegate>(nint address, params object[] arguments) where TDelegate : Delegate
    {
        Ensure.That(arguments.Length <= MaxArguments, () => $"Too many arguments. Only up to {MaxArguments} are supported.");
        Ensure.That(address != 0, () => "Invalid kernel function address.");

        using var patch = KernelPatch.Patch(_driver, _kernelNtAddAtomAddress, address);
        var function = Marshal.GetDelegateForFunctionPointer<TDelegate>(_ntAddAtomAddress);

        return (TResult)function.DynamicInvoke(arguments)!;
    }

    public static KernelFunctionCaller Create(DriverInterface driver, PortableExecutable pe, nint baseAddress)
    {
        var ntdllHandle = Kernel32.GetModuleHandle(NtdllLibraryName);

        Ensure.That(ntdllHandle != nint.Zero);

        var ntAddAtomAddress = Kernel32.GetProcAddress(ntdllHandle, NtAddAtomFunctionName);

        Ensure.That(ntAddAtomAddress != nint.Zero, () => "Failed to get export ntdll.NtAddAtom.");

        var kernelNtAddAtomFunction = pe.GetExportFunction(NtAddAtomFunctionName);
        var kernelNtAddAtomAddress = baseAddress + kernelNtAddAtomFunction.Address;

        return new KernelFunctionCaller(driver, kernelNtAddAtomAddress, ntAddAtomAddress);
    }
}