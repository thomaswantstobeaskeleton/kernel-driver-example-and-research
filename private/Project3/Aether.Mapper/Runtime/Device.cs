using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using Aether.Mapper.Core;
using Aether.Mapper.Exceptions;
using Aether.Mapper.Interop;

namespace Aether.Mapper.Runtime;

internal unsafe interface IDevice
{
    nint DeviceHandle { get; }

    void Request<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T>(uint code, T* request) where T : unmanaged;

    TResponse Request<
            [DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] TRequest,
            [DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] TResponse>
        (uint code, TRequest* request)
        where TRequest : unmanaged
        where TResponse : struct;
}

internal sealed unsafe class Device : IDevice, IDisposable
{
    private readonly DeviceHandle _handle;

    private Device(DeviceHandle handle)
    {
        _handle = handle;
    }

    public nint DeviceHandle => _handle;

    public void Request<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T>(uint code, T* request) where T : unmanaged
    {
        // Feeding fckn AOT
        Console.Write(string.Empty);

        var size = Marshal.SizeOf<T>();
        var result = Kernel32.DeviceIoControl(_handle, code, request, size, null, 0, out _, 0);
        Ensure.That(result, () => $"Failed device request: 0x{code:x} (Address: {((nint)request):x}, Size: {size})");
    }

    public TResponse Request<
        [DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] TRequest,
        [DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] TResponse>
        (uint code, TRequest* request)
        where TRequest : unmanaged
        where TResponse : struct
    {
        // Feeding fckn AOT
        Console.Write(string.Empty);

        var data = new byte[MarshalType<TResponse>.Size];
        var requestSize = MarshalType<TRequest>.Size;

        fixed (byte* pData = data)
        {
            var result = Kernel32.DeviceIoControl(_handle, code, request, requestSize, pData, data.Length, out var resultLength, 0);

            Ensure.That(result, () => $"Failed device request: 0x{code:x} (Size: {requestSize})");
            Ensure.That(resultLength == data.Length, () => "Device response output length is not equal to length of response type");
        }

        return MarshalType<TResponse>.ToValue(data);
    }

    public void Dispose()
    {
        _handle.Dispose();
    }

    public static Device Open(string deviceName)
    {
        var handle = Interop.DeviceHandle.Open(deviceName, FileAccess.ReadWrite);
        return new Device(handle);
    }

    public static bool Exists(string deviceName)
    {
        try
        {
            using var _ = Interop.DeviceHandle.Open(deviceName, FileAccess.Read);
            return true;
        }
        catch (NtStatusException e) when (e.Status is NtStatus.ObjectNameNotFound)
        {
            return false;
        }
    }
}