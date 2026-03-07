namespace Aether.Mapper.Exceptions;

internal abstract class EnumStatusException<TEnum>(TEnum status) : Exception(GetMessage(status)) where TEnum : Enum
{
    public TEnum Status { get; } = status;

    private static string GetMessage(TEnum status)
    {
        return Enum.IsDefined(typeof(TEnum), status)
            ? $"Operation failed with status: {status} (0x{status:x})"
            : $"Operation failed with status: 0x{status:x}";
    }
}