using System.Diagnostics.CodeAnalysis;
using System.Text;
using Aether.Mapper.Core;
using Aether.Mapper.Runtime;

namespace Aether.Mapper.PE32;

internal sealed class PortableExecutableBinary
{
    private const char NullTerminator = '\0';

    private readonly byte[] _data;

    private PortableExecutableBinary(byte[] data)
    {
        _data = data;
    }

    public T Read<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T>(int offset) where T : struct
    {
        var data = Read(offset, MarshalType<T>.Size);
        return MarshalType<T>.ToValue(data);
    }

    public string Read(int offset, Encoding encoding, int maxLength)
    {
        Ensure.That(offset < _data.Length);

        maxLength = offset + maxLength >= _data.Length ? _data.Length - offset : maxLength;

        var data = Read(offset, maxLength);
        var text = encoding.GetString(data);
        var ntIndex = text.IndexOf(NullTerminator);

        return ntIndex != -1 ? text.Remove(ntIndex) : text;
    }

    private byte[] Read(int offset, int length) => _data[offset..(offset + length)];

    public static PortableExecutableBinary Load(byte[] data) => new(data);
}