namespace Aether.Mapper.PE32;

internal enum ImageDirectoryEntry
{
    Export,
    Import,
    Resource,
    Exception,
    Security,
    BaseRelocation,
    Debug,
    Architecture,
    GlobalPtr,
    Tls,
    LoadConfig,
    BoundImport,
    IAT,
    DelayImport,
    ComDescriptor,
    Reserved,
}