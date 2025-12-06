using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace SimPreview
{
    public partial class MainWindow : Window
    {
        readonly List<SimCharacter> characters = new();
        readonly List<SimProjectile> projectiles = new();
        readonly DispatcherTimer tickTimer;
        readonly double tickDt = 1.0 / 30.0;
        // string? to avoid Nullable warnings
        readonly string? repoRoot;
        readonly string? gameDefsPath;
        readonly string? scriptsPath;
        readonly Dictionary<string, AbilityDef> abilities = new();

        int nextEntityId = 1000;

        public MainWindow()
        {
            InitializeComponent();

            repoRoot = FindRepoRoot();
            gameDefsPath = repoRoot != null ? System.IO.Path.Combine(repoRoot, "game", "game_defs.json") : null;
            scriptsPath = repoRoot != null ? System.IO.Path.Combine(repoRoot, "game", "scripts") : null;

            Log($"Repo root: {repoRoot ?? "(not found)"}");
            Log($"Game defs path: {gameDefsPath ?? "(none)"}");
            Log($"Scripts path: {scriptsPath ?? "(none)"}");

            LoadGameDefinitions();

            tickTimer = new DispatcherTimer();
            tickTimer.Interval = TimeSpan.FromMilliseconds(33);
            tickTimer.Tick += (s, e) => Tick();
            tickTimer.Start();

            BtnSpawn.Click += (s, e) => SpawnCharacter();
            BtnCast.Click += (s, e) => CastSelectedFireball();

            WorldCanvas.MouseLeftButtonDown += (s, e) =>
            {
                var p = e.GetPosition(WorldCanvas);
                SelectNearestCharacter(p);
            };
        }

        void Log(string msg)
        {
            LogOutput.Text = $"[{DateTime.Now:HH:mm:ss}] {msg}\n" + LogOutput.Text;
        }

        // string? to avoid Nullable warnings
        string? FindRepoRoot()
        {
            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            for (int i = 0; i < 8 && dir != null; ++i)
            {
                var gameDir = System.IO.Path.Combine(dir.FullName, "game");
                if (Directory.Exists(gameDir)) return dir.FullName;
                dir = dir.Parent;
            }
            return null;
        }

        void LoadGameDefinitions()
        {
            if (string.IsNullOrEmpty(gameDefsPath) || !File.Exists(gameDefsPath))
            {
                Log("No game_defs.json found. Using empty abilities set.");
                return;
            }

            try
            {
                string txt = File.ReadAllText(gameDefsPath);
                using var doc = JsonDocument.Parse(txt);

                // Try both array ("abilities") or dictionary ("abilities" object) forms
                // This should change ngl
                if (doc.RootElement.TryGetProperty("abilities", out var abilEl))
                {
                    if (abilEl.ValueKind == JsonValueKind.Array)
                    {
                        foreach (var e in abilEl.EnumerateArray())
                        {
                            var id = e.GetProperty("id").GetString() ?? "";
                            var script = e.GetProperty("script").GetString();
                            abilities[id] = new AbilityDef { Id = id, Script = script };
                        }
                    }
                    else if (abilEl.ValueKind == JsonValueKind.Object)
                    {
                        foreach (var prop in abilEl.EnumerateObject())
                        {
                            var id = prop.Name;
                            var val = prop.Value;
                            var script = val.GetProperty("script").GetString();
                            abilities[id] = new AbilityDef { Id = id, Script = script };
                        }
                    }
                }

                Log($"Loaded {abilities.Count} abilities.");
            }
            catch (Exception ex)
            {
                Log($"Failed to parse game_defs.json: {ex.Message}");
            }
        }

        // very small spawn
        void SpawnCharacter()
        {
            var rng = new Random();
            var x = 100 + rng.NextDouble() * (WorldCanvas.ActualWidth - 200);
            var y = 100 + rng.NextDouble() * (WorldCanvas.ActualHeight - 200);

            var c = new SimCharacter
            {
                Id = nextEntityId++,
                Name = "SimHero",
                PosX = x,
                PosY = y,
                Speed = 200.0 // world units per second in canvas coords
            };
            characters.Add(c);
            Log($"Spawned character {c.Id} at ({c.PosX:0.0},{c.PosY:0.0})");
            Redraw();
        }

        // SimCharacter? to avoid Nullable warnings
        SimCharacter? selected = null;
        void SelectNearestCharacter(System.Windows.Point p)
        {
            double best = double.MaxValue;
            SimCharacter? bestc = null; // SimCharacter? to avoid Nullable warnings
            foreach (var c in characters)
            {
                var dx = c.PosX - p.X;
                var dy = c.PosY - p.Y;
                var d = dx * dx + dy * dy;
                if (d < best) { best = d; bestc = c; }
            }
            selected = bestc;
            if (selected != null) Log($"Selected {selected.Id}");
            Redraw();
        }

        void CastSelectedFireball()
        {
            if (selected == null)
            {
                Log("No character selected.");
                return;
            }

            if (!abilities.ContainsKey("fireball_test"))
            {
                Log("Ability 'fireball_test' not defined in game_defs.json");
                return;
            }

            var mouse = Mouse.GetPosition(WorldCanvas);
            var ab = abilities["fireball_test"];
            var scriptPath = scriptsPath != null ? System.IO.Path.Combine(scriptsPath, ab.Script ?? "") : null;
            if (scriptPath == null || !File.Exists(scriptPath))
            {
                Log($"Lua script not found: {scriptPath ?? "(none)"}");
                return;
            }

            try
            {
                // Run MoonSharp script expecting function 'cast(caster_id, tx, ty)' returns table
                var code = File.ReadAllText(scriptPath);
                var script = new Script(CoreModules.Preset_Default);
                script.DoString(code);

                var res = script.Call(script.Globals.Get("cast"), DynValue.NewNumber(selected.Id), DynValue.NewNumber(mouse.X), DynValue.NewNumber(mouse.Y));
                if (res.Type != DataType.Table)
                {
                    Log("Lua 'cast' did not return a table.");
                    return;
                }

                // read fields with fallback defaults
                var dmg = res.Table.Get("damage").NumberOrNull() ?? 0.0;
                var speed = res.Table.Get("speed").NumberOrNull() ?? 300.0;
                var radius = res.Table.Get("radius").NumberOrNull() ?? 8.0;
                var tx = res.Table.Get("tx").NumberOrNull() ?? mouse.X;
                var ty = res.Table.Get("ty").NumberOrNull() ?? mouse.Y;

                var proj = new SimProjectile
                {
                    X = selected.PosX,
                    Y = selected.PosY,
                    TargetX = tx,
                    TargetY = ty,
                    Speed = speed,
                    Radius = radius,
                    Damage = dmg
                };
                projectiles.Add(proj);
                Log($"Cast fireball from {selected.Id} -> ({tx:0.0},{ty:0.0}) speed {speed:0.0} dmg {dmg:0.0}");
            }
            catch (Exception ex)
            {
                Log($"Lua error: {ex.Message}");
            }
        }

        void Tick()
        {
            var toRemove = new List<SimProjectile>();
            double dt = tickDt;

            // update projectiles each tick
            foreach (var p in projectiles)
            {
                p.Tick(dt);
                if (p.IsFinished) toRemove.Add(p);
            }
            foreach (var r in toRemove) projectiles.Remove(r);

            // redraw
            Redraw();
        }

        void Redraw()
        {
            WorldCanvas.Children.Clear();

            foreach (var p in projectiles)
            {
                var ellipse = new Ellipse { Width = p.Radius * 2, Height = p.Radius * 2, Fill = Brushes.OrangeRed };
                Canvas.SetLeft(ellipse, p.X - p.Radius);
                Canvas.SetTop(ellipse, p.Y - p.Radius);
                WorldCanvas.Children.Add(ellipse);
            }

            foreach (var c in characters)
            {
                var ellipse = new Ellipse
                {
                    Width = 30,
                    Height = 30,
                    Fill = (c == selected) ? Brushes.LimeGreen : Brushes.LightBlue
                };
                Canvas.SetLeft(ellipse, c.PosX - 15);
                Canvas.SetTop(ellipse, c.PosY - 15);
                WorldCanvas.Children.Add(ellipse);
            }
        }
    }

    // HELPERS
    class SimCharacter
    {
        public int Id;
        // string? to avoid Nullable warnings
        public string? Name;
        public double PosX;
        public double PosY;
        public double Speed;
    }

    class SimProjectile
    {
        public double X;
        public double Y;
        public double TargetX;
        public double TargetY;
        public double Speed;
        public double Radius;
        public double Damage;
        public bool IsFinished = false;

        public void Tick(double dt)
        {
            var dx = TargetX - X;
            var dy = TargetY - Y;
            var dist = Math.Sqrt(dx * dx + dy * dy);
            if (dist < 1.0)
            {
                IsFinished = true;
                return;
            }
            var nx = dx / dist;
            var ny = dy / dist;
            X += nx * Speed * dt;
            Y += ny * Speed * dt;
        }
    }

    class AbilityDef
    {
        // string? to avoid Nullable warnings
        public string? Id;
        public string? Script;
    }

    static class DynValueExtensions
    {
        public static double? NumberOrNull(this DynValue v)
        {
            if (v == null) return null;
            if (v.Type == DataType.Number) return v.Number;
            return null;
        }
    }
}