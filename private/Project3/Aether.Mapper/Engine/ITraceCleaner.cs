namespace Aether.Mapper.Engine;

internal interface ITraceCleaner
{
    Task CleanAsync(CancellationToken cancellationToken);
}