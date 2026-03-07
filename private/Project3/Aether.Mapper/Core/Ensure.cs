using System.Diagnostics.CodeAnalysis;
using Aether.Mapper.Exceptions;
using Aether.Mapper.Interop;

namespace Aether.Mapper.Core;

internal static class Ensure
{
    public static void That([DoesNotReturnIf(false)] bool condition)
    {
        if (!condition)
            throw new InvalidOperationException();
    }

    public static void That([DoesNotReturnIf(false)] bool condition, Func<string> getMessage)
    {
        if (!condition)
            throw new InvalidOperationException(getMessage());
    }

    public static void ThatSuccess(NtStatus status)
    {
        if (status is not NtStatus.Success)
            throw new NtStatusException(status);
    }
}