using Aether.Mapper.Core;
using Aether.Mapper.Extensions;
using Aether.Mapper.Kernel;

namespace Aether.Mapper.Runtime;

internal sealed record PhysicalAddressInfo(nint Address, nint PageTableEntryPhysicalAddress, MMPTE PageTableEntry);

internal sealed class PhysicalAddressProvider
{
    private const int PageSize = 0x1000;

    private ulong _cr3;

    public PhysicalAddressInfo GetPhysicalAddress(DriverInterface driver, nint address)
    {
        GetSystemCr3(driver);

        var result = PageTable.Pml4.GetPhysicalAddress(driver, address, _cr3);

        return result;
    }

    private void GetSystemCr3(DriverInterface driver)
    {
        if (_cr3 is 0)
            _cr3 = GetSystemDirBase(driver);

        Ensure.That(_cr3 != 0, () => "Failed to initialize System cr3");
    }

    private static ulong GetSystemDirBase(DriverInterface driver)
    {
        for (var i = 0; i < 10; i++)
        {
            var virtualAddress = driver.MapIoSpace(i * 0x10000, 0x10000);

            try
            {
                for (var offset = 0; offset < 0x10000; offset += PageSize)
                {
                    var address = virtualAddress + offset;

                    if (0x1000600E9 != (0xffffffffffff00ff & driver.Read<ulong>(address)))
                        continue;

                    if (0xfffff80000000000 != (0xfffff80000000000 & driver.Read<ulong>(address + 0x70)))
                        continue;

                    if ((0xffffff0000000fff & driver.Read<ulong>(address + 0xa0)) != 0)
                        continue;

                    return driver.Read<uint>(address + 0xa0);
                }
            }
            finally
            {
                driver.UnmapIoSpace(virtualAddress);
            }
        }

        return 0;
    }

    private sealed class PageTable(int shift, PageTable? child)
    {
        private const ulong ZeroLowerMask = 0xfffffffffffff000UL;
        private const ulong ZeroHigherMask = 0x000fffffffffffffUL;
        private const int IndexMask = 0x1ff;
        private const int BitsPerLevel = 9;

        private const int PtIndexShift = 12;
        private const int PdIndexShift = PtIndexShift + BitsPerLevel;       // 21
        private const int PdptIndexShift = PdIndexShift + BitsPerLevel;     // 30
        private const int Pml4IndexShift = PdptIndexShift + BitsPerLevel;   // 39

        private static readonly PageTable Pt = new(PtIndexShift, null);
        private static readonly PageTable Pd = new(PdIndexShift, Pt);
        private static readonly PageTable Pdpt = new(PdptIndexShift, Pd);

        public static readonly PageTable Pml4 = new(Pml4IndexShift, Pdpt);

        public PhysicalAddressInfo GetPhysicalAddress(DriverInterface driver, nint address, ulong entryBase)
        {
            var virtualAddress = (ulong)address;
            var index = (virtualAddress >> shift) & IndexMask;
            var (entryAddress, pte) = ReadTableEntry(driver, entryBase, index);

            if (!IsPresent(pte.Value))
                return new PhysicalAddressInfo(0, nint.Zero, default);

            var entry = pte.Value & ZeroHigherMask;

            if (pte.LargePage || child is null)
            {
                var pageMask = (1UL << shift) - 1UL;
                var pageFrame = entry & ~pageMask;
                var pageOffset = virtualAddress & pageMask;

                return new((pageFrame + pageOffset).ToPointer(), entryAddress, pte);
            }

            Ensure.That(child is not null);

            return child.GetPhysicalAddress(driver, address, entry);
        }

        private static (nint, MMPTE) ReadTableEntry(DriverInterface driver, ulong entryBase, ulong index)
        {
            // Bits 11..0 reserved for flags
            var tableBase = entryBase & ZeroLowerMask;
            var entryAddress = (tableBase + (index << 3)).ToPointer();
            var pte = driver.ReadPhysical<MMPTE>(entryAddress);

            return (entryAddress, pte);
        }

        private static bool IsPresent(ulong entry) => (entry & 1) != 0;
    }
}