using System.Runtime.InteropServices;

namespace Aether.Mapper.Interop;

internal static unsafe class Ntdll
{
    public const uint FileAttributeNormal = 0x80;

    private const string LibraryName = "ntdll.dll";

    [DllImport(LibraryName, ExactSpelling = true, SetLastError = true)]
    public static extern NtStatus NtCreateFile(out nint handle, 
        FileAccess access,
        ObjectAttributes* objectAttributes, 
        IoStatusBlock* statusBlock,
        nint allocationSize,
        uint fileAttributes, 
        FileShare share,
        CreateDisposition createDisposition, 
        uint createOptions, 
        nint eaBuffer, 
        uint eaLength);

    [DllImport(LibraryName)]
    public static extern NtStatus NtClose(nint handle);

    [DllImport(LibraryName)]
    public static extern NtStatus RtlAdjustPrivilege(
        Privilege privilege,
        bool enablePrivilege,
        bool currentThread,
        out bool enabled
    );

    [DllImport(LibraryName)]
    public static extern NtStatus NtLoadDriver(UnicodeString* driverDeviceName);

    [DllImport(LibraryName)]
    public static extern NtStatus NtUnloadDriver(UnicodeString* driverDeviceName);

    [DllImport(LibraryName)]
    public static extern NtStatus NtQuerySystemInformation(
        SystemInformationClass systemInformationClass,
        nint systemInformation,
        int systemInformationLength,
        out int returnLength
    );
}