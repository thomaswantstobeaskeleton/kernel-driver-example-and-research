using System.Runtime.InteropServices;

namespace Aether.Mapper.Interop;

internal static class Kernel32
{
    private const string LibraryName = "kernel32.dll";

    [DllImport(LibraryName, SetLastError = true)]
    public static extern nint GetModuleHandle(string lpModuleName);

    [DllImport(LibraryName, SetLastError = true)]
    public static extern nint GetProcAddress(nint hModule, string procName);

    [DllImport(LibraryName, ExactSpelling = true, SetLastError = true, CharSet = CharSet.Auto)]
    public static extern unsafe bool DeviceIoControl(nint hDevice, uint dwIoControlCode, void* lpInBuffer, int nInBufferSize, void* lpOutBuffer, int nOutBufferSize, out long lpBytesReturned, uint lpOverlapped);
}