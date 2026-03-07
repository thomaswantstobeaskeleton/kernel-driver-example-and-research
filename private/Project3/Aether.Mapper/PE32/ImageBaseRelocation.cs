using System.Runtime.InteropServices;

namespace Aether.Mapper.PE32;

[StructLayout(LayoutKind.Sequential)]
internal struct ImageBaseRelocation
{
    public int VirtualAddress;
    public int SizeOfBlock;

    public static int StructSize = Marshal.SizeOf<ImageBaseRelocation>();
}