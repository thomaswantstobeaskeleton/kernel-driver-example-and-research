namespace Aether.Mapper.Core.Logging;

internal interface ILoggerTransport
{
    Task WriteAsync(string message, CancellationToken cancellationToken);
}