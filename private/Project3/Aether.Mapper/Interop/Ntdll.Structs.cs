using System.Runtime.InteropServices;
using Aether.Mapper.Core;

namespace Aether.Mapper.Interop;

[StructLayout(LayoutKind.Sequential, Pack = 0)]
internal unsafe struct ObjectAttributes
{
    public static readonly int Size = Marshal.SizeOf<ObjectAttributes>();

    public int Length;
    public nint RootDirectory;
    public UnicodeString* ObjectName;
    public ObjectAttributeFlags Attributes;
    public nint SecurityDescriptor;
    public nint SecurityQualityOfService;
}

[StructLayout(LayoutKind.Sequential)]
internal struct UnicodeString : IDisposable
{
    public ushort Length;
    public ushort MaximumLength;
    public nint Buffer;

    public void Dispose()
    {
        Marshal.FreeHGlobal(Buffer);
        Length = 0;
        MaximumLength = 0;
    }

    public static UnicodeString Create(string text)
    {
        Ensure.That(text.Length < ushort.MaxValue);

        var length = (ushort)(text.Length << 1);

        return new UnicodeString
        {
            Length = length,
            MaximumLength = (ushort)(length + 2),
            Buffer = Marshal.StringToHGlobalUni(text)
        };
    }
}

[StructLayout(LayoutKind.Sequential)]
internal struct IoStatusBlock
{
    public uint Status;
    public nint Information;
}

[StructLayout(LayoutKind.Sequential)]
internal struct RtlProcessModuleInformation
{
    public nint Section;
    public nint MappedBase;
    public nint ImageBase;
    public uint ImageSize;
    public uint Flags;
    public ushort LoadOrderIndex;
    public ushort InitOrderIndex;
    public ushort LoadCount;
    public ushort OffsetToFileName;

    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
    public string FullPathName;

    public static readonly int Size = Marshal.SizeOf<RtlProcessModuleInformation>();
}

[StructLayout(LayoutKind.Sequential)]
internal struct RtlProcessModules
{
    public uint NumberOfModules;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1)]
    public RtlProcessModuleInformation[] Modules;
}

[StructLayout(LayoutKind.Sequential)]
internal struct SystemHandle
{
    public nint Object;
    public long UniqueProcessId;
    public nint HandleValue;
    public uint GrantedAccess;
    public ushort CreatorBackTraceIndex;
    public ushort ObjectTypeIndex;
    public uint HandleAttributes;
    public uint Reserved;

    public static readonly int Size = Marshal.SizeOf<SystemHandle>();
}

[StructLayout(LayoutKind.Sequential)]
internal struct SystemHandleInformationEx
{
    public long HandleCount;
    public long Reserved;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1)]
    public SystemHandle[] Handles;
}