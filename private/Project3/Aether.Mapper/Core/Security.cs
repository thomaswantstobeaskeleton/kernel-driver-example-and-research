namespace Aether.Mapper.Core;

internal static class Security
{
    public static byte[] Xor(byte[] data, uint xor)
    {
        var result = new byte[data.Length];
        var xorBytes = BitConverter.GetBytes(xor);

        for (var i = 0; i < data.Length; i++)
        {
            result[i] = xorBytes.Aggregate(data[i], (current, xorValue) => (byte)(current ^ xorValue));
        }

        return result;
    }
}