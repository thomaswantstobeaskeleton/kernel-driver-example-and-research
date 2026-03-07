using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using System.Text;
using Aether.Mapper.Runtime;

namespace Aether.Mapper.PE32;

internal sealed class PortableExecutable
{
    private readonly PortableExecutableBinary _binary;
    private readonly ImageDataDirectory[] _dataDirectory;

    private PortableExecutable(PortableExecutableBinary binary)
    {
        _binary = binary;

        DosHeader = binary.Read<ImageDosHeader>(0);
        FileHeader = binary.Read<ImageFileHeader>(PeTool.GetFileHeaderOffset(DosHeader));

        var optionalHeaderOffset = PeTool.GetOptionalHeaderOffset(DosHeader);

        OptionalHeader = binary.Read<ImageOptionalHeader64>(optionalHeaderOffset);

        var dataDirectoryOffset = (int)Marshal.OffsetOf<ImageOptionalHeader64>(nameof(ImageOptionalHeader64.DataDirectory));

        _dataDirectory = new ImageDataDirectory[OptionalHeader.NumberOfRvaAndSizes];

        for (var i = 0; i < _dataDirectory.Length; i++)
        {
            _dataDirectory[i] = binary.Read<ImageDataDirectory>(optionalHeaderOffset + dataDirectoryOffset + i * Marshal.SizeOf<ImageDataDirectory>());
        }
    }

    private ImageDosHeader DosHeader { get; }
    public ImageFileHeader FileHeader { get; }
    public ImageOptionalHeader64 OptionalHeader { get; }
    public ImageDataDirectory this[ImageDirectoryEntry entry] => _dataDirectory[(int)entry];
    public ImageSectionHeader this[ImageDataDirectory directory] => EnumerateSections().First(section => directory.VirtualAddress >= section.VirtualAddress && directory.VirtualAddress < section.VirtualAddress + section.VirtualSize);

    public ImageSectionHeader GetSection(string name) => EnumerateSections().First(s => s.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
    public IReadOnlyCollection<ImageSectionHeader> GetSections() => EnumerateSections().ToArray();

    public ImageFunction GetExportFunction(string name) => EnumerateExportFunctions().First(f => f.Name.Equals(name, StringComparison.OrdinalIgnoreCase));

    public T Read<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T>(int offset) where T : struct => _binary.Read<T>(offset);
    public string ReadText(int offset) => _binary.Read(offset, Encoding.UTF8, 256);

    private IEnumerable<ImageSectionHeader> EnumerateSections()
    {
        var sectionsNumber = FileHeader.NumberOfSections;
        var offset = PeTool.GetOptionalHeaderOffset(DosHeader) + FileHeader.SizeOfOptionalHeader;

        for (var i = 0; i < sectionsNumber; i++)
        {
            yield return _binary.Read<ImageSectionHeader>(offset);
            offset += ImageSectionHeader.StructSize;
        }
    }

    private IEnumerable<ImageFunction> EnumerateExportFunctions()
    {
        var directory = this[ImageDirectoryEntry.Export];

        if (directory.VirtualAddress == 0) yield break;

        var section = this[directory];
        var table = Read<ImageExportDirectory>(RvaToFileOffset(section, directory.VirtualAddress));

        var namesOffset = RvaToFileOffset(section, table.AddressOfNames);
        var functionsOffset = RvaToFileOffset(section, table.AddressOfFunctions);
        var ordinalsOffset = RvaToFileOffset(section, table.AddressOfNameOrdinals);

        for (var i = 0; i < table.NumberOfNames; i++)
        {
            var nameRva = Read<int>(namesOffset + i * sizeof(int));
            var functionName = ReadText(RvaToFileOffset(section, nameRva));
            var ordinalIndex = Read<ushort>(ordinalsOffset + i * sizeof(ushort));
            var functionRva = Read<int>(functionsOffset + ordinalIndex * sizeof(int));
            var functionOffset = RvaToFileOffset(section, functionRva);

            yield return new ImageFunction(functionName, functionRva, functionOffset);
        }
    }

    public static PortableExecutable Load(byte[] data) => new(PortableExecutableBinary.Load(data));

    private static int RvaToFileOffset(ImageSectionHeader section, int rva) => PeTool.RvaToFileOffset(section, rva);
}