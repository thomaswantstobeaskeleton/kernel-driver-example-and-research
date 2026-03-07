using Wise.CLI;

namespace Aether.Mapper;

[CliOptions(generateHelp: false)]
public partial class Options
{
    [CliOption("filePath", 'f', required: true, description: "Path to the driver file to map")]
    public string FilePath { get; set; } = string.Empty;

    [CliOption("logConsole", 'c', description: "Enable console log")]
    public bool LogConsole { get; set; }

    [CliOption("logFile", 'l', description: "File path for log")]
    public string? LogFile { get; set; }
}