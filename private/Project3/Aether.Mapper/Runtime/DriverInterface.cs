using System.Diagnostics.CodeAnalysis;
using System.Text;

namespace Aether.Mapper.Runtime;

internal abstract unsafe class DriverInterface
{
    private const char NullTerminator = '\0';

    public abstract uint Timestamp { get; }
    public abstract string DriverName { get; }
    public abstract string DriverPath { get; }
    public abstract nint DeviceHandle { get; }

    public T Read<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T>(nint address) where T : struct
        => MarshalType<T>.ToValue(Read(address, MarshalType<T>.Size));

    public string Read(nint address, Encoding encoding, int maxLength)
    {
        var data = Read(address, maxLength);
        var text = encoding.GetString(data);
        var ntIndex = text.IndexOf(NullTerminator);

        return ntIndex != -1 ? text.Remove(ntIndex) : text;
    }

    public byte[] Read(nint address, int length)
    {
        var data = new byte[length];

        fixed (byte* pData = data)
        {
            CopyMemory(address, (nint)pData, length);
        }

        return data;
    }

    public T ReadPhysical<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T>(nint address) where T : struct
    {
        var virtualAddress = MapIoSpace(address, MarshalType<T>.Size);

        try
        {
            return Read<T>(virtualAddress);
        }
        finally
        {
            UnmapIoSpace(virtualAddress);
        }
    }

    public void Write<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T>
        (nint address, T value) where T : unmanaged
        => Write(address, MarshalType<T>.ToBytes(value));

    public void WritePhysical<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T>(nint address, T value) where T : unmanaged
    {
        var virtualAddress = MapIoSpace(address, MarshalType<T>.Size);

        try
        {
            Write(virtualAddress, value);
        }
        finally
        {
            UnmapIoSpace(virtualAddress);
        }
    }

    public void WriteReadonly(nint address, byte[] data)
    {
        var physicalAddress = GetPhysicalAddress(address);
        var virtualAddress = MapIoSpace(physicalAddress.Address, data.Length);

        try
        {
            Write(virtualAddress, data);
        }
        finally
        {
            UnmapIoSpace(virtualAddress);
        }
    }

    public void Write(nint address, byte[] data)
    {
        fixed (byte* pData = data)
        {
            CopyMemory((nint)pData, address, data.Length);
        }
    }

    public abstract nint MapIoSpace(nint physicalAddress, int size);
    public abstract void UnmapIoSpace(nint address);
    public abstract PhysicalAddressInfo GetPhysicalAddress(nint address);
    protected abstract void CopyMemory(nint sourceAddress, nint targetAddress, int length);
}