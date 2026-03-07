using Aether.Mapper.Interop;

namespace Aether.Mapper.Exceptions;

internal sealed class NtStatusException(NtStatus status) : EnumStatusException<NtStatus>(status);