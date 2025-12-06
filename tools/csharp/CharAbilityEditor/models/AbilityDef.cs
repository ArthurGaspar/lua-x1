using System.Text.Json.Serialization;
namespace CharAbilityEditor;

public class AbilityDef
{
    public string Id { get; set; } = "";
    public string DisplayName { get; set; } = "";
    public float Cooldown { get; set; }
    [JsonPropertyName("script")] // Maps JSON "script" to C# "ScriptName"
    public string ScriptName { get; set; } = "";
}