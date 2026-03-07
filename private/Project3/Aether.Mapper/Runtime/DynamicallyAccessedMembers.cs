using System.Diagnostics.CodeAnalysis;

namespace Aether.Mapper.Runtime;

internal static class DynamicallyAccessedMembers
{
    public const DynamicallyAccessedMemberTypes DefaultTypes = DynamicallyAccessedMemberTypes.PublicConstructors |
                                                               DynamicallyAccessedMemberTypes.NonPublicConstructors |
                                                               DynamicallyAccessedMemberTypes.PublicFields;
}