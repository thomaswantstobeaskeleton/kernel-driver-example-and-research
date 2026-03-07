using System.Runtime.InteropServices;
using Aether.Mapper.Core;

namespace Aether.Mapper.Interop;

internal static class SystemInformation
{
    public static nint GetModuleImageBase(string name)
    {
        Ensure.ThatSuccess(Ntdll.RtlAdjustPrivilege(Privilege.Debug, true, false, out _));

        var allocated = QuerySystemModuleInformation(SystemInformationClass.SystemModuleInformation);

        try
        {
            var modulesCount = Marshal.ReadInt32(allocated);
            var modulePointer = allocated + Marshal.OffsetOf<RtlProcessModules>(nameof(RtlProcessModules.Modules)).ToInt32();

            for (var i = 0; i < modulesCount; i++)
            {
                var module = Marshal.PtrToStructure<RtlProcessModuleInformation>(modulePointer);
                var moduleName = module.FullPathName[module.OffsetToFileName..];

                if (name.Equals(moduleName, StringComparison.OrdinalIgnoreCase))
                    return module.ImageBase;

                modulePointer += RtlProcessModuleInformation.Size;
            }

            throw new Exception($"Module not found: {name}");
        }
        finally
        {
            Marshal.FreeHGlobal(allocated);
        }
    }

    public static nint GetObjectByDeviceHandle(nint deviceHandle)
    {
        var allocated = QuerySystemModuleInformation(SystemInformationClass.SystemExtendedHandleInformation);

        try
        {
            var handlesCount = Marshal.ReadInt64(allocated);
            var handlePointer = allocated + Marshal.OffsetOf<SystemHandleInformationEx>(nameof(SystemHandleInformationEx.Handles)).ToInt32();

            for (var i = 0; i < handlesCount; i++)
            {
                var handle = Marshal.PtrToStructure<SystemHandle>(handlePointer);

                if (handle.UniqueProcessId == Environment.ProcessId && handle.HandleValue == deviceHandle)
                    return handle.Object;

                handlePointer += SystemHandle.Size;
            }

            throw new Exception($"Handle not found. (ProcessId: {Environment.ProcessId}, DeviceHandle: {deviceHandle})");
        }
        finally
        {
            Marshal.FreeHGlobal(allocated);
        }
    }

    private static nint QuerySystemModuleInformation(SystemInformationClass informationClass)
    {
        NtStatus status;

        var allocated = nint.Zero;
        var bufferLength = 0;

        do
        {
            if (bufferLength != 0)
            {
                if (allocated != nint.Zero)
                {
                    Marshal.FreeHGlobal(allocated);
                }

                allocated = Marshal.AllocHGlobal(bufferLength);
            }

            status = Ntdll.NtQuerySystemInformation(informationClass, allocated, bufferLength, out bufferLength);
        } while (status is NtStatus.StatusInfoLengthMismatch);

        try
        {
            Ensure.ThatSuccess(status);
            return allocated;
        }
        catch
        {
            if (allocated != nint.Zero)
                Marshal.FreeHGlobal(allocated);

            throw;
        }
    }
}