using System.Runtime.InteropServices;

namespace Aether.Mapper.PE32;

[StructLayout(LayoutKind.Sequential)]
internal struct ImageDataDirectory
{
    public int VirtualAddress;
    public int Size;
}