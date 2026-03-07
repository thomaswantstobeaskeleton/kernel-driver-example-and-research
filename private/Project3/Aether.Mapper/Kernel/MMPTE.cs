using Aether.Mapper.Extensions;
using System.Runtime.InteropServices;

namespace Aether.Mapper.Kernel;

[StructLayout(LayoutKind.Sequential)]
public struct MMPTE
{
    public ulong Value;

    public bool Valid { get => Value.GetBit(0); set => Value = Value.SetBit(0, value); }
    public bool Write { get => Value.GetBit(1); set => Value = Value.SetBit(1, value); }
    public bool Owner { get => Value.GetBit(2); set => Value = Value.SetBit(2, value); }
    public bool WriteThrough { get => Value.GetBit(3); set => Value = Value.SetBit(3, value); }
    public bool CacheDisable { get => Value.GetBit(4); set => Value = Value.SetBit(4, value); }
    public bool Accessed { get => Value.GetBit(5); set => Value = Value.SetBit(5, value); }
    public bool Dirty { get => Value.GetBit(6); set => Value = Value.SetBit(6, value); }
    public bool LargePage { get => Value.GetBit(7); set => Value = Value.SetBit(7, value); }
    public bool Global { get => Value.GetBit(8); set => Value = Value.SetBit(8, value); }
    public bool NoExecute { get => Value.GetBit(63); set => Value = Value.SetBit(63, value); }
    public ulong PageFrameNumber
    {
        get => (Value >> 12) & 0xFFFFFFFFF;
        set
        {
            Value &= ~(0xFFFFFFFFFUL << 12);
            Value |= (value & 0xFFFFFFFFFUL) << 12;
        }
    }
}