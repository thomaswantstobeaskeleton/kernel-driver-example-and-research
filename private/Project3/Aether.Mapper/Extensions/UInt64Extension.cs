namespace Aether.Mapper.Extensions;

internal static class UInt64Extension
{
    public static bool GetBit(this ulong value, int index) => (value & (1UL << index)) != 0;
    public static ulong SetBit(this ulong value, int index, bool b) => b ? value | 1UL << index : value & ~(1UL << index);
    public static nint ToPointer(this ulong value) => unchecked((nint)value);
}