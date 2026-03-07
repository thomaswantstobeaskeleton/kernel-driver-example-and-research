using System.Collections.Concurrent;
using System.Text;
using NeoSmart.AsyncLock;

namespace Aether.Mapper.Core.Logging;

internal static class Log
{
    private static readonly AsyncLock Lock = new();
    private static readonly ConcurrentBag<ILoggerTransport> Transports = [];

    public static void AddTransport(ILoggerTransport transport)
    {
        Transports.Add(transport);
    }

    public static async ValueTask MessageAsync(string message, CancellationToken cancellationToken = default)
    {
        await WriteAllTransportsAsync(Format(message), cancellationToken);
    }

    public static void Message(string message) => MessageAsync(message).GetAwaiter().GetResult();

    public static async ValueTask ExceptionAsync(Exception exception, CancellationToken cancellationToken = default)
    {
        var builder = new StringBuilder();

        for (var e = exception; e != null; e = e.InnerException)
        {
            builder.Append(Format($"Error: {e.Message}"));

            if (!string.IsNullOrEmpty(exception.StackTrace))
                builder.Append(Format($"Stack: {e.StackTrace}"));
        }

        await WriteAllTransportsAsync(builder.ToString(), cancellationToken);
    }

    public static void Exception(Exception exception) => ExceptionAsync(exception).GetAwaiter().GetResult();

    private static async Task WriteAllTransportsAsync(string message, CancellationToken cancellationToken)
    {
        if (Transports.IsEmpty) return;

        using (await Lock.LockAsync(cancellationToken))
        {
            var tasks = Transports.Select(t => t.WriteAsync(message, cancellationToken));
            await Task.WhenAll(tasks);
        }
    }

    private static string Format(string message)
    {
        var builder = new StringBuilder();
        builder.Append($"[{DateTime.Now:dd/MM/yyyy HH:mm:ss.fff}] ");
        builder.AppendLine(message);

        return builder.ToString();
    }
}