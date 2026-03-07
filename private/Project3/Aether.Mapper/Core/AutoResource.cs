namespace Aether.Mapper.Core;

internal sealed class AutoResource(Action dispose) : IDisposable
{
    public void Dispose() => dispose();
}