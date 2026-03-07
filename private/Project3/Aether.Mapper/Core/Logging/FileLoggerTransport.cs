using System.Text;

namespace Aether.Mapper.Core.Logging;

internal sealed class FileLoggerTransport : ILoggerTransport, IAsyncDisposable
{
    private readonly StreamWriter _writer;

    public FileLoggerTransport(string filePath)
    {
        filePath = Path.GetFullPath(filePath);

        _writer = CreateLogFile(filePath);
    }

    public async Task WriteAsync(string message, CancellationToken cancellationToken)
    {
        await _writer.WriteAsync(message.AsMemory(), cancellationToken);
    }

    private static StreamWriter CreateLogFile(string filePath)
    {
        var directoryPath = Path.GetDirectoryName(filePath);

        Ensure.That(!string.IsNullOrEmpty(directoryPath));
        Directory.CreateDirectory(directoryPath);

        var options = new FileStreamOptions
        {
            Mode = FileMode.Create,
            Access = FileAccess.Write,
            Share = FileShare.Read
        };

        var writer = new StreamWriter(filePath, Encoding.UTF8, options);
        writer.AutoFlush = true;

        return writer;
    }

    public async ValueTask DisposeAsync()
    {
        await _writer.DisposeAsync();
    }
}