using System;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using Aether.Mapper.Core;
using Aether.Mapper.Core.Logging;
using Aether.Mapper.Interop;
using Aether.Mapper.Kernel;
using Aether.Mapper.PE32;
using Aether.Mapper.Runtime;

namespace Aether.Mapper.Engine;

internal sealed unsafe class Mapper(DriverInterface driver, NtoskrnlModule ntoskrnlModule)
{
    private const int ImageScnCntUninitializedData = 0x00000080;
    private const int ImageRelBasedDir64 = 10;
    private const ulong DefaultSecurityCookie = 0x2b992ddfa232;

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate NtStatus DriverEntry(ulong a1, ulong a2);

    public void Map(string filePath)
    {
        var data = File.ReadAllBytes(filePath);
        var pe = PortableExecutable.Load(data);
        var imageSize = pe.OptionalHeader.SizeOfImage;
        var localImage = new byte[imageSize];

        var headersSize = pe.GetSections().First().VirtualAddress;

        imageSize -= headersSize;

        Log.Message($"[Mapper] Allocated size: {imageSize} bytes");

        var grayZoneAddress = FindGrayZoneAddress(imageSize);

        Log.Message($"[Mapper] GrayZone address: 0x{grayZoneAddress:x}");

        var kernelImageBase = ntoskrnlModule.AllocatePool(PoolType.NonPagedPool, imageSize);
        var displacedKernelImageBase = kernelImageBase - (int)headersSize;

        try
        {
            fixed (byte* localImageBase = localImage)
            {
                MapImage((nint)localImageBase, data, pe);
                MapRelocations(pe, (nint)localImageBase, displacedKernelImageBase);
                FixSecurityCookie(pe, (nint)localImageBase, displacedKernelImageBase);
                MapImports(pe, (nint)localImageBase);
            }

            driver.Write(kernelImageBase, localImage[(int)headersSize..]);

            var entryPoint = displacedKernelImageBase + (int)pe.OptionalHeader.AddressOfEntryPoint;

            Log.Message($"[Mapper] Calling entry point: 0x{entryPoint:x}");

            var result = ntoskrnlModule.CallCore<NtStatus, DriverEntry>(entryPoint, 0ul, 0ul);

            if (result != NtStatus.Success)
            {
                ntoskrnlModule.FreePool(kernelImageBase);
                Log.Message("Kernel allocation freed.");
            }

            Log.Message($"[Mapper] Calling entry point result: {result}");
        }
        catch (Exception e)
        {
            ntoskrnlModule.FreePool(kernelImageBase);
            Log.Exception(e);
        }
    }

    private nint FindGrayZoneAddress(uint size)
    {
        var pages = (size + Memory.PageSize - 1) / Memory.PageSize;
        var consecutive = 0;
        var start = nint.Zero;

        for (var address = unchecked((nint)0xFFFFF80000000000); address < unchecked((nint)0xFFFFF88000000000); address += Memory.PageSize)
        {
            if (!ntoskrnlModule.IsAddressValid(address))
            {
                if (consecutive == 0)
                    start = address;

                consecutive++;

                if (consecutive >= pages)
                    return start;
            }
            else
            {
                consecutive = 0;
                start = 0;
            }
        }

        return nint.Zero;
    }

    private static void MapImage(nint address, byte[] data, PortableExecutable pe)
    {
        Marshal.Copy(data, 0, address, (int)pe.OptionalHeader.SizeOfHeaders);

        var sections = pe.GetSections();

        foreach (var section in sections)
        {
            if ((section.Characteristics & ImageScnCntUninitializedData) > 0) continue;

            var sectionVirtualAddress = address + (int)section.VirtualAddress;

            Marshal.Copy(data, section.PointerToRawData, sectionVirtualAddress, section.SizeOfRawData);
        }

        Console.WriteLine("[Mapper] Image header and sections successfully mapped");
    }

    private static void MapRelocations(PortableExecutable pe, nint localImageBase, nint kernelImageBase)
    {
        var directory = pe[ImageDirectoryEntry.BaseRelocation];

        if (directory.VirtualAddress is 0) return;

        var section = pe[directory];
        var offset = PeTool.RvaToFileOffset(section, directory.VirtualAddress);
        var delta = (ulong)kernelImageBase - pe.OptionalHeader.ImageBase;

        while (true)
        {
            var relocation = pe.Read<ImageBaseRelocation>(offset);

            if (relocation.SizeOfBlock is 0) break;

            var count = (relocation.SizeOfBlock - ImageBaseRelocation.StructSize) >> 1;
            var relocationsOffset = offset + ImageBaseRelocation.StructSize;

            for (var i = 0; i < count; i++)
            {
                var entry = pe.Read<ushort>(relocationsOffset + (i << 1));
                var type = entry >> 12;

                if (type is 0) continue; // Skip IMAGE_REL_BASED_ABSOLUTE

                Ensure.That(type == ImageRelBasedDir64, () => "Unsupported relocation");

                var patchRva = relocation.VirtualAddress + (entry & 0xfff);

                *(ulong*)(localImageBase + patchRva) += delta;
            }

            offset += relocation.SizeOfBlock;
        }

        Log.Message("[Mapper] Relocations successfully mapped");
    }

    private static void FixSecurityCookie(PortableExecutable pe, nint localImageBase, nint kernelImageBase)
    {
        var directory = pe[ImageDirectoryEntry.LoadConfig];

        if (directory.VirtualAddress is 0) return;

        var config = Marshal.PtrToStructure<ImageLoadConfigDirectory>(localImageBase + directory.VirtualAddress);
        var securityCookiePointer = (ulong*)(config.SecurityCookie - (ulong)kernelImageBase + (ulong)localImageBase);
        var securityCookieValue = *securityCookiePointer;

        Ensure.That(securityCookieValue == DefaultSecurityCookie, () => "SecurityCookie is not correct");

        *securityCookiePointer = BitConverter.ToUInt64(RandomNumberGenerator.GetBytes(8));

        Log.Message($"[Mapper] Applied new security cookie: 0x{(*securityCookiePointer):x}");
    }

    private void MapImports(PortableExecutable pe, nint localImageBase)
    {
        var directory = pe[ImageDirectoryEntry.Import];

        if (directory.VirtualAddress == 0) return;

        var section = pe[directory];
        var offset = PeTool.RvaToFileOffset(section, directory.VirtualAddress);
        var descriptor = pe.Read<ImageImportDescriptor>(offset);

        while (descriptor.FirstThunk != 0)
        {
            var moduleNameOffset = PeTool.RvaToFileOffset(section, descriptor.Name);
            var moduleName = pe.ReadText(moduleNameOffset);

            if (moduleName.Equals("hal.dll", StringComparison.OrdinalIgnoreCase))
            {
                moduleName = "ntoskrnl.exe";
            }

            var module = KernelModule.Load(driver, moduleName);

            var firstThunk = (ImageThunkData64*)(localImageBase + descriptor.FirstThunk);
            var originalFirstThunk = (ImageThunkData64*)(localImageBase + descriptor.OriginalFirstThunk);

            while (originalFirstThunk->Function != 0)
            {
                var thunkDataOffset = PeTool.RvaToFileOffset(section, (int)originalFirstThunk->AddressOfData);
                var name = pe.ReadText(thunkDataOffset + (int)Marshal.OffsetOf<ImageImportByName>(nameof(ImageImportByName.Name)));
                var function = module.PE.GetExportFunction(name);
                var address = module.BaseAddress + function.Address;

                firstThunk->Function = (ulong)address;

                firstThunk++;
                originalFirstThunk++;
            }

            offset += ImageImportDescriptor.StructSize;
            descriptor = pe.Read<ImageImportDescriptor>(offset);
        }

        Log.Message($"[Mapper] Imports successfully mapped");
    }
}