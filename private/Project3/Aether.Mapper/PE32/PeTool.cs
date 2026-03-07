namespace Aether.Mapper.PE32;

internal static class PeTool
{
    private const int RelativeFileHeaderOffset = 0x4;

    public static int GetFileHeaderOffset(ImageDosHeader dosHeader) => dosHeader.e_lfanew + RelativeFileHeaderOffset;
    public static int GetOptionalHeaderOffset(ImageDosHeader dosHeader) => GetFileHeaderOffset(dosHeader) + ImageFileHeader.StructSize;

    public static int RvaToFileOffset(ImageSectionHeader section, int rva)
    {
        return section.PointerToRawData + (int)(rva - section.VirtualAddress);
    }
}