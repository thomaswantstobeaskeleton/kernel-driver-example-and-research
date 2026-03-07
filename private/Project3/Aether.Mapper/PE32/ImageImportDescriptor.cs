using System.Runtime.InteropServices;

namespace Aether.Mapper.PE32;

[StructLayout(LayoutKind.Sequential)]
internal struct ImageImportDescriptor
{
    public uint OriginalFirstThunk; // RVA to original unbound IAT (PIMAGE_THUNK_DATA)
                                    // Can also represent Characteristics field (0 for null descriptor)
    public uint TimeDateStamp;      // 0 if not bound, -1 if bound and has real date/time stamp
                                    // O.W. date/time stamp of DLL bound to
    public int ForwarderChain;     // -1 if no forwarders
    public int Name;               // RVA to name of DLL
    public int FirstThunk;         // RVA to IAT (if bound, this IAT has actual addresses)

    public static int StructSize = Marshal.SizeOf<ImageImportDescriptor>();
}