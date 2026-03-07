using Aether.Mapper.Core;

namespace Aether.Mapper.Runtime;

internal sealed class KernelPatch(DriverInterface driver, nint address, byte[] originalData) : IDisposable
{
    public void Dispose()
    {
        driver.WriteReadonly(address, originalData);
    }

    public static KernelPatch Patch(DriverInterface driver, nint patchAddress, nint functionAddress)
    {
        var patchData = CreatePatchData(functionAddress);
        var originalData = driver.Read(patchAddress, patchData.Length);
        var isPatched = originalData[..2].SequenceEqual(patchData[..2]) &&
                        originalData[^2..].SequenceEqual(patchData[^2..]);

        Ensure.That(!isPatched, () => "The function is already patched. Another instance of the mapper might be running.");

        driver.WriteReadonly(patchAddress, patchData);

        return new KernelPatch(driver, patchAddress, originalData);
    }

    private static byte[] CreatePatchData(nint functionAddress)
    {
        var patchData = new byte[]
        {
            0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, [KernelFunctionAddress]
            0xFF, 0xE0                                                  // jmp rax
        };

        Buffer.BlockCopy(MarshalType<nint>.ToBytes(functionAddress), 0, patchData, 2, nint.Size);

        return patchData;
    }
}