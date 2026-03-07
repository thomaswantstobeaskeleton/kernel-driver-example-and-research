using System.Runtime.InteropServices;

namespace Aether.Mapper.PE32;

[StructLayout(LayoutKind.Explicit)]
internal struct ImageThunkData64
{
    [FieldOffset(0)]
    public ulong ForwarderString;

    [FieldOffset(0)]
    public ulong Function;

    [FieldOffset(0)]
    public ulong Ordinal;

    [FieldOffset(0)]
    public ulong AddressOfData;
}