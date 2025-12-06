using System.Diagnostics;
using System.IO;
using System.Text.Json;

namespace CharAbilityEditor;

public class JsonDatabase
{
    private readonly string _path;
    private readonly Action<string> _log;

    public JsonDatabase(string path, Action<string> log = null)
    {
        _path = Path.GetFullPath(path);
        _log = log ?? (msg => Console.WriteLine(msg));
    }

    private void Log(string msg) => _log($"[JsonDatabase] {msg}");

    /// <summary>
    /// Loads game definitions from a JSON file specified at construction time.
    /// </summary>
    public GameDefinitions Load()
    {
        Log($"[JsonDatabase] Attempting to load from: {_path}");

        if (!File.Exists(_path))
        {
            Log($"[JsonDatabase] ERROR: File does not exist at path: {_path}");
            throw new FileNotFoundException($"Configuration file not found at: {_path}");
        }

        string json = File.ReadAllText(_path);
        Log($"[JsonDatabase] File read successfully. Length: {json.Length} characters.");

        try
        {
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            var result = JsonSerializer.Deserialize<GameDefinitions>(json, options);
            if (result == null)
            {
                Log("[JsonDatabase] WARNING: Deserialized object is null.");
                throw new InvalidDataException("The JSON was parsed but resulted in a null GameDefinitions instance. Check format and required fields.");
            }
            return result;
        }
        catch (JsonException jex)
        {
            Log($"[JsonDatabase] JSON Parsing Error: {jex.Message}");
            throw; // Re-throw to be caught by MainWindow
        }
    }

    /// <summary>
    /// Saves the given game definitions to the configured JSON file.
    /// </summary>
    public void Save(GameDefinitions defs)
    {
        Log($"[JsonDatabase] Saving definitions to: {_path}");
        string json = JsonSerializer.Serialize(defs, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(_path, json);
        Log("[JsonDatabase] Save completed.");
    }
}