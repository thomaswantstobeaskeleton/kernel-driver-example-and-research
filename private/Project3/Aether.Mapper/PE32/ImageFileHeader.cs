using System.Runtime.InteropServices;

namespace Aether.Mapper.PE32;

[StructLayout(LayoutKind.Sequential)]
internal struct ImageFileHeader
{
    public ushort Machine;
    public ushort NumberOfSections;
    public uint TimeDateStamp;
    public uint PointerToSymbolTable;
    public uint NumberOfSymbols;
    public ushort SizeOfOptionalHeader;
    public ushort Characteristics;

    public static readonly int StructSize = Marshal.SizeOf<ImageFileHeader>();
}
