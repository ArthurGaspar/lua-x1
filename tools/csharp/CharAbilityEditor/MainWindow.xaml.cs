using Microsoft.Win32;
using System;
using System.Diagnostics;
using System.IO;
using System.Windows;

namespace CharAbilityEditor;

/// <summary>
/// Main editor window for managing character abilities and syncing Lua scripts.
/// Automatically resolves project paths relative to repo structure.
/// </summary>
public partial class MainWindow : Window
{
    // empty or ignorenull (null!) to avoid Nullable warnings
    private readonly string GameDefsPath = "";
    private readonly string ScriptsPath = "";
    private JsonDatabase JsonDb = null!;
    private ScriptPusher ScriptPusher = null!;

    public MainWindow()
    {
        InitializeComponent();

        try
        {
            // Automatically locate game folder from repo layout
            // Added null check safety for Parent access
            var currentDir = Directory.GetCurrentDirectory();
            var dirInfo = Directory.GetParent(currentDir)?.Parent?.Parent;
            
            if (dirInfo == null)
            {
                Console.WriteLine("[MainWindow] ERROR: Could not resolve repo root. Directory structure might be unexpected.");
                MessageBox.Show("Critical Error: Could not resolve repository root path.");
                Application.Current.Shutdown();
                return;
            }

            string repoRoot = dirInfo.FullName;
            Console.WriteLine($"[MainWindow] Repo Root resolved to: {repoRoot}");

            GameDefsPath = Path.Combine(repoRoot, "game", "game_defs.json");
            ScriptsPath = Path.Combine(repoRoot, "game", "scripts", "imported");

            Console.WriteLine($"[MainWindow] GameDefsPath: {GameDefsPath}");
            Console.WriteLine($"[MainWindow] ScriptsPath: {ScriptsPath}");

            // In MainWindow constructor, after creating JsonDb and ScriptPusher:
            Action<string> logger = msg => Console.WriteLine($"[MainWindow->Child] {msg}");
            JsonDb = new JsonDatabase(GameDefsPath);
            ScriptPusher = new ScriptPusher(ScriptsPath);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[MainWindow] Constructor Error: {ex}");
            MessageBox.Show($"Startup Error: {ex.Message}");
        }
    }

    /// <summary>
    /// Loads game definitions from JSON using <see cref="JsonDatabase"/>.
    /// </summary>
    private void LoadGameDefs_Click(object sender, RoutedEventArgs e)
    {
        Console.WriteLine("[MainWindow] 'Load Game Definitions' clicked.");
        try
        {
            var defs = JsonDb.Load();
            Console.WriteLine($"[MainWindow] Successfully loaded definitions. Chars: {defs.Characters.Count}, Abilities: {defs.Abilities.Count}");
            MessageBox.Show($"Loaded {defs.Characters.Count} characters and {defs.Abilities.Count} abilities.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[MainWindow] ERROR loading definitions: {ex}");
            MessageBox.Show($"Failed to load definitions.\n\nError: {ex.Message}\n\nCheck the Output window for full details.");
        }
    }

    private void PushScripts_Click(object sender, RoutedEventArgs e)
    {
        Console.WriteLine("[MainWindow] 'Push Scripts' clicked.");
        try
        {
            ScriptPusher.PushAllScripts();
            Console.WriteLine("[MainWindow] Scripts push completed successfully.");
            MessageBox.Show("Scripts pushed to game/scripts.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[MainWindow] ERROR pushing scripts: {ex}");
            MessageBox.Show($"Failed to push scripts.\n\nError: {ex.Message}");
        }
    }
}