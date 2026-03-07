using Aether.Mapper.Core;
using Aether.Mapper.Engine;
using Aether.Mapper.Interop;
using Aether.Mapper.PE32;

namespace Aether.Mapper.Runtime;

internal sealed record Signature(int Offset, string Pattern);

internal sealed class KernelModule
{
    private readonly Dictionary<string, SectionInfo> _loadedSections = new();
    private readonly DriverInterface _driver;

    private KernelModule(DriverInterface driver, PortableExecutable pe, nint baseAddress)
    {
        _driver = driver;
        PE = pe;
        BaseAddress = baseAddress;
    }

    public PortableExecutable PE { get; }
    public nint BaseAddress { get; }

    public nint GetAddress(string sectionName, Signature[] signatures)
    {
        Ensure.That(TryScan(sectionName, signatures, out var address));
        return ResolveRelativeAddress(address);
    }

    public bool TryScan(string sectionName, Signature[] signatures, out nint address)
    {
        var info = GetOrLoadSectionInfo(sectionName);

        foreach (var signature in signatures)
        {
            if (PatternScanner.TryScan(info.Data, signature.Pattern, out var offset))
            {
                address = info.Address + offset + signature.Offset;
                return true;
            }
        }

        address = nint.Zero;
        return false;
    }

    public nint ResolveRelativeAddress(nint address)
    {
        const int relativeAddressSize = 4;

        var relativeAddress = _driver.Read<int>(address);

        return address + relativeAddressSize + relativeAddress;
    }

    private SectionInfo GetOrLoadSectionInfo(string name)
    {
        if (!_loadedSections.TryGetValue(name, out var info))
        {
            var section = PE.GetSection(name);
            var sectionAddress = BaseAddress + (int)section.VirtualAddress;
            var sectionData = _driver.Read(sectionAddress, section.VirtualSize);

            info = new SectionInfo(sectionAddress, sectionData);

            _loadedSections.Add(name, info);
        }

        return info;
    }

    public static KernelModule Load(DriverInterface driver, string name)
    {
        var moduleImageBase = SystemInformation.GetModuleImageBase(name);
        var modulePayloadSize = ResolveSizeToRead(driver, moduleImageBase);
        var moduleData = driver.Read(moduleImageBase, modulePayloadSize);
        var pe = PortableExecutable.Load(moduleData);

        return new KernelModule(driver, pe, moduleImageBase);
    }

    private static int ResolveSizeToRead(DriverInterface driver, nint moduleBase)
    {
        var dosHeader = driver.Read<ImageDosHeader>(moduleBase);
        var optionalHeader = driver.Read<ImageOptionalHeader64>(moduleBase + PeTool.GetOptionalHeaderOffset(dosHeader));

        return (int)optionalHeader.SizeOfInitializedData;
    }

    private sealed record SectionInfo(nint Address, byte[] Data);
}