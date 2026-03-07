namespace Aether.Mapper.Engine.Symbols;

internal static class SymbolServer
{
    private const string MicrosoftSymbolServer = "http://msdl.microsoft.com/download/symbols";

    public static async Task<byte[]> DownloadAsync(string pdbFileName, string index)
    {
        var url = $"{MicrosoftSymbolServer}/{pdbFileName}/{index}/{pdbFileName}";
        
        using var client = new HttpClient();
        return await client.GetByteArrayAsync(url);
    }
}