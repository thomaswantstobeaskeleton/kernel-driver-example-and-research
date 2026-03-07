using Aether.Mapper.Core;
using Microsoft.Win32;

namespace Aether.Mapper.Extensions;

internal static class RegistryKeyExtension
{
    public static RegistryKey OpenRequiredKey(this RegistryKey source, string path)
    {
        var key = source.OpenSubKey(path, writable: true);

        Ensure.That(key != null, () => $@"Registry key not found: {source.Name}\{path}");

        return key;
    }
}