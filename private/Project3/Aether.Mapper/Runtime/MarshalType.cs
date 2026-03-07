using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Aether.Mapper.Core;

namespace Aether.Mapper.Runtime;

// This class is implemented using the static cache pattern
// It is expected to have different field for different parameters of T
[SuppressMessage("ReSharper", "StaticMemberInGenericType")]
public static class MarshalType<[DynamicallyAccessedMembers(DynamicallyAccessedMembers.DefaultTypes)] T> where T : struct
{
    private static readonly TypeCode TypeCode;
    public static readonly int Size;

    static MarshalType()
    {
        var realType = typeof(T);

        if (realType.IsEnum)
        {
            realType = realType.GetEnumUnderlyingType();
        }

        Size = Unsafe.SizeOf<T>();
        TypeCode = Type.GetTypeCode(realType);
    }

    public static byte[] ToBytes(T value)
    {
        var boxed = value as object;

        Ensure.That(boxed != null);

        var data = new byte[Size];

        MemoryMarshal.Write(data, in value);

        return data;
    }

    public static unsafe T ToValue(Memory<byte> data)
    {
        switch (TypeCode)
        {
            case TypeCode.Object: break;
            case TypeCode.Byte:
            case TypeCode.SByte: return (T)(object)data.Span[0];
            case TypeCode.Char: return (T)(object)(char)data.Span[0];
            case TypeCode.Boolean:
            case TypeCode.Int16:
            case TypeCode.UInt16:
            case TypeCode.Int32:
            case TypeCode.UInt32:
            case TypeCode.Int64:
            case TypeCode.UInt64:
            case TypeCode.Single:
            case TypeCode.Double:
            case TypeCode.Decimal:
            case TypeCode.DateTime: return MemoryMarshal.Read<T>(data.Span);
            default: throw new InvalidCastException("Conversion not support");
        }

        fixed (byte* pData = data.Span)
        {
            return Marshal.PtrToStructure<T>((nint)pData);
        }
    }
}