using System.Runtime.InteropServices;

namespace Aether.Mapper.PE32;

[StructLayout(LayoutKind.Sequential)]
internal struct ImageImportByName
{
    public ushort Hint;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
    public string Name;
}