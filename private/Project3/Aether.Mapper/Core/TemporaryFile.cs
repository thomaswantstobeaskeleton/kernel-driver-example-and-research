using Aether.Mapper.Core.Logging;
using System.Security.Cryptography;

namespace Aether.Mapper.Core;

internal sealed class TemporaryFile(string path) : IDisposable
{
    public string Path { get; } = path;

    public void Dispose()
    {
        var file = new FileInfo(Path);

        if (file.Exists)
        {
            // This is how data is protected from recovery
            // File.WriteAllBytes opens the file in 'Truncate' mode
            // The new section will not be used because we don't exceed original file size

            try
            {
                File.WriteAllBytes(Path, RandomNumberGenerator.GetBytes((int)file.Length));
                File.Delete(Path);
            }
            catch (Exception e)
            {
                Log.Message($"Error during file cleanup: {e.Message}");
            }
        }
    }

    public static TemporaryFile Create(byte[] data, string fileName)
    {
        var directoryPath = Environment.GetEnvironmentVariable("TEMP");

        Ensure.That(directoryPath != null);

        var filePath = System.IO.Path.Combine(directoryPath, fileName);

        Ensure.That(!File.Exists(filePath), () => $"File is already exists: {filePath}");

        File.WriteAllBytes(filePath, data);

        return new TemporaryFile(filePath);
    }
}