using System.Runtime.InteropServices;

namespace Aether.Mapper.PE32;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal struct ImageDebugDirectory
{
    public uint Characteristics;
    public uint TimeDateStamp;
    public ushort MajorVersion;
    public ushort MinorVersion;
    public uint Type;
    public uint SizeOfData;
    public int AddressOfRawData;
    public uint PointerToRawData;
}