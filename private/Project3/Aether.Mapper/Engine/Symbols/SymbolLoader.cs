using Aether.Mapper.Core;
using Aether.Mapper.PE32;
using System.Runtime.InteropServices;

namespace Aether.Mapper.Engine.Symbols;

internal sealed class SymbolLoader
{
    private const string DriversDirectoryName = "drivers";
    private const string CvSignatureType = "RSDS";
    private const int MaxPdbFileLength = 0x104;
    private static readonly string SystemDirectory = Environment.SystemDirectory;

    public static async Task<string> LoadAsync(string fileName)
    {
        var info = GetCvInfo(fileName);
        var index = GetIndex(info);

        var cachedPdbPath = Path.Combine(Environment.CurrentDirectory, $"{Path.GetFileNameWithoutExtension(info.PdbFileName)}_{index}{Path.GetExtension(info.PdbFileName)}");

        if (!File.Exists(cachedPdbPath))
        {
            var data = SymbolServer.DownloadAsync(info.PdbFileName, index).GetAwaiter().GetResult();
            await File.WriteAllBytesAsync(cachedPdbPath, data);
        }

        return cachedPdbPath;
    }

    private static string GetIndex(CvInfoPdb70 info) => $"{info.Signature:N}{info.Age}";

    private static CvInfoPdb70 GetCvInfo(string fileName)
    {
        var filePath = GetFilePath(fileName);
        var pe = PortableExecutable.Load(File.ReadAllBytes(filePath));
        var directory = pe[ImageDirectoryEntry.Debug];

        Ensure.That(directory.VirtualAddress != 0, () => $"Directory is not valid: {ImageDirectoryEntry.Debug}");

        var section = pe[directory];
        var entry = pe.Read<ImageDebugDirectory>(PeTool.RvaToFileOffset(section, directory.VirtualAddress));
        var info = pe.Read<CvInfoPdb70>(PeTool.RvaToFileOffset(section, entry.AddressOfRawData));

        Ensure.That(info.CvSignature.Equals(CvSignatureType, StringComparison.Ordinal));

        return info;
    }

    private static string GetFilePath(string fileName)
    {
        var filePath = Path.GetFullPath(Path.Combine(Environment.SystemDirectory, fileName));

        if (!File.Exists(filePath))
        {
            filePath = Path.GetFullPath(Path.Combine(SystemDirectory, DriversDirectoryName, fileName));
            Ensure.That(File.Exists(filePath), () => $"File not exists: {filePath}");
        }

        return filePath;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    private struct CvInfoPdb70
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 4)]
        public string CvSignature;
        public Guid Signature;
        public uint Age;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = MaxPdbFileLength)]
        public string PdbFileName;
    }
}