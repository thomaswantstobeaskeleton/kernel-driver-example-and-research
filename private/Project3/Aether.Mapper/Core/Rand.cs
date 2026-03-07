using System.Security.Cryptography;

namespace Aether.Mapper.Core;

internal static class Rand
{
    private const string AlphaNumericSymbols = "0123456789abcdefghijklmnopqrstuvwxyz";

    public static string GenerateName(int count)
    {
        var first = GetRandomSymbol(isLetterOnly: true);
        var rest = new string(Enumerable.Range(0, count - 1)
            .Select(_ => GetRandomSymbol(isLetterOnly: false))
            .ToArray());

        return $"{first}{rest}";
    }

    private static char GetRandomSymbol(bool isLetterOnly)
    {
        return AlphaNumericSymbols[RandomNumberGenerator.GetInt32(isLetterOnly ? 10 : 0, AlphaNumericSymbols.Length)];
    }
}