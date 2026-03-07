using Aether.Mapper.Core;
using Aether.Mapper.Engine.Symbols;
using Aether.Mapper.Extensions;
using Aether.Mapper.Kernel;
using Aether.Mapper.Runtime;
using System.Runtime.InteropServices;

namespace Aether.Mapper.Engine;

internal sealed class MemoryProvider(DriverInterface driver, NtoskrnlModule module, ISymbolProvider symbolProvider)
{
    private const int PageShift = 12;

    public async Task<nint> GetMemoryAsync(int size, CancellationToken cancellationToken)
    {
        await Task.Delay(1000);

        var pagesSize = (int)Memory.AlignToPageSize((ulong)size);
        var mdl = module.AllocatePagesForMdl(nint.Zero, unchecked((nint)ulong.MaxValue), nint.Zero, pagesSize);

        Ensure.That(mdl != nint.Zero, () => "Failed to allocate MDL.");

        var virtualAddress = module.MapLockedPagesSpecifyCache(mdl,
            KProcessorMode.KernelMode,
            MemoryCachingType.MmCached,
            nint.Zero,
            false,
            MmPagePriority.NormalPagePriority);

        if (virtualAddress == nint.Zero)
        {
            module.FreePagesFromMdl(mdl);
            module.FreeMdl(mdl);
            throw new Exception("Failed to map MDL pages.");
        }

        // Initialize memory with zeros
        driver.Write(virtualAddress, new byte[pagesSize]);

        await Task.Delay(2000);

        var pfnDatabase = module.BaseAddress + await symbolProvider.GetOffsetAsync("ntoskrnl.exe", "MmPfnDatabase", cancellationToken);

        for (var offset = 0; offset < pagesSize; offset += Memory.PageSize)
        {
            var address = virtualAddress + offset;

            driver.Write(virtualAddress, new byte[Memory.PageSize]);

            var physicalAddress = driver.GetPhysicalAddress(address);
            var pte = physicalAddress.PageTableEntry;
            
            pte.Accessed = false;
            pte.Dirty = false;
            pte.Global = false;

            driver.WritePhysical(physicalAddress.PageTableEntryPhysicalAddress, pte);

            var pa = module.GetPhysicalAddress(address);
            var pfnIndex = (ulong)pa >> PageShift;
            var entry = pfnDatabase + (nint)(pfnIndex * 0x28);

            Ensure.That(module.GetPhysicalAddress(entry) != nint.Zero, () => $"Failed to get physical address for PFN entry at 0x{entry:x}");

            var pfn = driver.Read<MMPFN>(entry);

        }

        return mdl;
    }

    public static nint MiGetPteAddress(ulong virtualAddress)
    {
        return unchecked((nint)(((virtualAddress >> 9) & 0x7FFFFFFFF8) + 0xFFFFF68000000000));
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MDL
    {
        public nint Next;
        public ushort Size;
        public ushort MdlFlags;
        public nint Process;
        public nint MappedSystemVa;
        public nint StartVa;
        public uint ByteCount;
        public uint ByteOffset;
        // PFN array начинается сразу после этой структуры
    }

    [StructLayout(LayoutKind.Explicit, Size = 0x30)]
    public struct MMPFN
    {
        [FieldOffset(0x00)]
        public ulong Flink; // u1.Flink or LIST_ENTRY / TreeNode stubbed

        [FieldOffset(0x08)]
        public ulong PteAddress; // union: PTE pointer / ulong

        [FieldOffset(0x10)]
        public MMPTE OriginalPte;

        [FieldOffset(0x18)]
        public ulong u2; // _MIPFNBLINK stubbed

        [FieldOffset(0x20)]
        public ushort ReferenceCount;

        [FieldOffset(0x22)]
        public byte e1;

        [FieldOffset(0x23)]
        public byte e3; // overlapped with e1

        [FieldOffset(0x24)]
        public uint u5; // _MI_PFN_ULONG5 stubbed

        [FieldOffset(0x28)]
        public ulong u4; // bitfield - requires manual accessors

        // Convenience accessors for u4
        public ulong PteFrame => u4 & 0xFFFFFFFFFF;
        public bool ResidentPage => (u4 >> 40 & 1) != 0;
        public bool PfnExists => (u4 >> 52 & 1) != 0;
        public byte PageIdentity => (byte)((u4 >> 57) & 0x7);
    }

    [StructLayout(LayoutKind.Explicit, Size = 0x58)]
    public struct MMPFNList
    {
        [FieldOffset(0x00)]
        public ulong Total;

        [FieldOffset(0x08)]
        public ulong ListName;

        [FieldOffset(0x10)]
        public nint Flink;

        [FieldOffset(0x18)]
        public nint Blink;

        [FieldOffset(0x20)]
        public int Lock;

        [FieldOffset(0x28)]
        public MMPFN EmbeddedPfn;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MMPTE
    {
        public ulong Value;

        public bool Valid { get => Value.GetBit(0); set => Value = Value.SetBit(0, value); }
        public bool Write { get => Value.GetBit(1); set => Value = Value.SetBit(1, value); }
        public bool Owner { get => Value.GetBit(2); set => Value = Value.SetBit(2, value); }
        public bool WriteThrough { get => Value.GetBit(3); set => Value = Value.SetBit(3, value); }
        public bool CacheDisable { get => Value.GetBit(4); set => Value = Value.SetBit(4, value); }
        public bool Accessed { get => Value.GetBit(5); set => Value = Value.SetBit(5, value); }
        public bool Dirty { get => Value.GetBit(6); set => Value = Value.SetBit(6, value); }
        public bool LargePage { get => Value.GetBit(7); set => Value = Value.SetBit(7, value); }
        public bool Global { get => Value.GetBit(8); set => Value = Value.SetBit(8, value); }
        public bool NoExecute { get => Value.GetBit(63); set => Value = Value.SetBit(63, value); }
        public ulong PageFrameNumber
        {
            get => (Value >> 12) & 0xFFFFFFFFF;
            set
            {
                Value &= ~(0xFFFFFFFFFUL << 12);
                Value |= (value & 0xFFFFFFFFFUL) << 12;
            }
        }
    }
}