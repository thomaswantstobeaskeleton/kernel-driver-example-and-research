namespace Aether.Mapper.Interop;

internal enum Privilege
{
    LoadDriver = 10,
    Debug = 20
}

internal enum CreateDisposition : uint
{
    Supersede,
    Open,
    Create,
    OpenIf,
    Overwrite,
    OverwriteIf
}

internal enum ObjectAttributeFlags : uint
{
    None = 0,
    CaseInsensitive = 0x40
}

internal enum SystemInformationClass
{
    SystemModuleInformation = 11,
    SystemExtendedHandleInformation = 64
}

internal enum NtStatus : uint
{
    // Success
    Success = 0x00000000,
    Wait0 = 0x00000001,
    Wait1 = 0x00000002,
    Wait2 = 0x00000003,
    Wait3 = 0x00000004,
    Abandoned = 0x00000080,
    AbandonedWait0 = 0x00000080,
    UserApc = 0x000000C0,
    AlreadyComplete = 0x000000FF,
    KernelApc = 0x00000100,

    // Errors
    DataTypeMisalignment = 0x80000002,
    InvalidFunction = 0xC0000001,
    FileNotFound = 0xC0000002,
    PathNotFound = 0xC0000003,
    StatusInfoLengthMismatch = 0xC0000004,
    AccessDenied = 0xC0000005,
    InvalidHandle = 0xC0000008,
    InvalidParameter = 0xC000000D,
    NoMemory = 0xC0000017,
    PrivilegeNotHeld = 0xC0000061,
    NotSupported = 0xC00000BB,

    // Object
    ObjectNameNotFound = 0xC0000034,
    ObjectNameInvalid = 0xC0000033,
    ObjectPathInvalid = 0xC0000039,
    ObjectPathNotFound = 0xC000003A,

    // Device states
    DeviceBusy = 0xC00000E8,
    DeviceNotReady = 0xC00000A3,

    // IO
    EndOfFile = 0xC0000011,
    FileExists = 0xC0000043,
    SharingViolation = 0xC0000044,

    // Common
    BufferTooSmall = 0xC0000023,
    NameTooLong = 0xC0000106,
    NoMoreFiles = 0x80000006,
    NotImplemented = 0xC0000002,

    // System state
    Timeout = 0x00000102,
    Reparse = 0x00000104,
    Pending = 0x00000103,

    // Registry
    RegistryKeyDeleted = 0xC0000138,
    RegistryCorrupt = 0xC00000FD,
    RegistryIoFailed = 0xC00000FE,

    // Other
    FileIsADirectory = 0xC00000BA,
    VolumeDirty = 0xC00000DF,

    // Unknown
    UnknownStatus = 0xFFFFFFFF
}