using Aether.Mapper.Core;

namespace Aether.Mapper.Interop;

internal sealed class DeviceHandle : IDisposable
{
    private readonly nint _handle;

    private DeviceHandle(nint handle)
    {
        _handle = handle;
    }

    public void Dispose()
    {
        Ntdll.NtClose(_handle);
    }

    public static unsafe DeviceHandle Open(string deviceName, FileAccess access)
    {
        using var name = UnicodeString.Create(deviceName);

        var attributes = new ObjectAttributes
        {
            Length = ObjectAttributes.Size,
            RootDirectory = nint.Zero,
            ObjectName = &name,
            Attributes = ObjectAttributeFlags.None
        };

        var statusBlock = new IoStatusBlock();

        Ensure.ThatSuccess(Ntdll.NtCreateFile(out var handle,
            access,
            &attributes,
            &statusBlock,
            nint.Zero,
            Ntdll.FileAttributeNormal,
            FileShare.ReadWrite,
            CreateDisposition.Open,
            0,
            nint.Zero,
            0));

        Ensure.That(handle != nint.Zero);

        return new DeviceHandle(handle);
    }

    public static implicit operator nint(DeviceHandle handle) => handle._handle;
}