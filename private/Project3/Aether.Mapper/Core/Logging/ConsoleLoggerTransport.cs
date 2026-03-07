namespace Aether.Mapper.Core.Logging;

internal sealed class ConsoleLoggerTransport : ILoggerTransport
{
    public Task WriteAsync(string message, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        Console.Write(message);
        return Task.CompletedTask;
    }
}