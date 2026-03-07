using Aether.Mapper.Extensions;

namespace Aether.Mapper.Core;

internal static class Memory
{
    private const ulong PageMask = PageSize - 1;
    public const int PageSize = 0x1000; // 4Kb

    public static ulong AlignToPageSize(ulong value) => (value + PageMask) & ~PageMask;
}