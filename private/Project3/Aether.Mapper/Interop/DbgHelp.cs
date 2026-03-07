using System.Runtime.InteropServices;

namespace Aether.Mapper.Interop;

internal static class DbgHelp
{
    private const string LibraryName = "dbghelp.dll";

    [DllImport(LibraryName, SetLastError = true)]
    public static extern bool SymInitialize(IntPtr hProcess, string? UserSearchPath, bool fInvadeProcess);

    [DllImport(LibraryName, SetLastError = true)]
    public static extern ulong SymLoadModuleEx(
        IntPtr hProcess,
        IntPtr hFile,
        string imageName,
        string? moduleName,
        ulong baseOfDll,
        uint dllSize,
        IntPtr data,
        uint flags);

    [DllImport(LibraryName, SetLastError = true)]
    public static extern bool SymUnloadModule64(nint hProcess, ulong baseOfDll);

    [DllImport(LibraryName, SetLastError = true, CharSet = CharSet.Ansi)]
    public static extern bool SymFromName(IntPtr hProcess, string name, IntPtr symbol);
}