namespace Aether.Mapper.Core;

internal sealed class Disposables : IDisposable
{
    private readonly Stack<IDisposable> _disposables = [];

    public T Add<T>(T item) where T : IDisposable
    {
        _disposables.Push(item);
        return item;
    }

    public void Dispose()
    {
        while (_disposables.Count > 0)
        {
            _disposables.Pop().Dispose();
        }
    }
}