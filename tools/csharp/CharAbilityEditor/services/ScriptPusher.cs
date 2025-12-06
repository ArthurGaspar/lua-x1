using System;
using System.Diagnostics;
using System.IO;
namespace CharAbilityEditor;

/// <summary>
/// Utility to deploy Lua script files from development folder to runtime destination.
/// </summary>
public class ScriptPusher
{
    private readonly string _targetScriptsPath;
    private readonly Action<string> _log;

    public ScriptPusher(string path, Action<string> log = null)
    {
        _targetScriptsPath = path;
        _log = log ?? (msg => Console.WriteLine(msg));
    }

    private void Log(string msg) => _log($"[JsonDatabase] {msg}");

    /// <summary>
    /// Copies all Lua scripts from the local 'scripts_src' directory to the configured target directory.
    /// Creates source/target folders if they don't exist.
    /// </summary>
    public void PushAllScripts()
    {
        string src = Path.Combine(Environment.CurrentDirectory, "scripts_src");
        Log($"[ScriptPusher] Source Directory: {src}");
        Log($"[ScriptPusher] Target Directory: {_targetScriptsPath}");

        if (!Directory.Exists(src))
        {
            Log("[ScriptPusher] Source directory 'scripts_src' does not exist. Creating it.");
            Directory.CreateDirectory(src);
        }

        if (!Directory.Exists(_targetScriptsPath))
        {
             Log("[ScriptPusher] Target directory does not exist. Creating it.");
             Directory.CreateDirectory(_targetScriptsPath);
        }

        string[] files = Directory.GetFiles(src, "*.lua");
        Log($"[ScriptPusher] Found {files.Length} lua files to copy.");

        foreach (string file in files)
        {
            string filename = Path.GetFileName(file);
            string dest = Path.Combine(_targetScriptsPath, filename);
            
            Log($"[ScriptPusher] Copying {filename} -> {dest}");
            File.Copy(file, dest, overwrite: true);
        }
        
        Log("[ScriptPusher] Push operation finished.");
    }
}