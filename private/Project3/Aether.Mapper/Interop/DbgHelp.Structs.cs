using System.Runtime.InteropServices;

namespace Aether.Mapper.Interop;

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct SymbolInfo
{
	public uint SizeOfStruct;
	public uint TypeIndex;
	public ulong Reserved1;
	public ulong Reserved2;
	public uint Index;
	public uint Size;
	public ulong ModBase;
	public uint Flags;
	public ulong Value;
	public ulong Address;
	public uint Register;
	public uint Scope;
	public uint Tag;
	public uint NameLen;
	public uint MaxNameLen;
	[MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1)]
	public string Name;
}