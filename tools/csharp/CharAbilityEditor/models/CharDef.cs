using System.Text.Json.Serialization;
namespace CharAbilityEditor;

public class CharDef
{
    public string Id { get; set; } = "";
    public string DisplayName { get; set; } = "";
    public float MaxHealth { get; set; }
    [JsonPropertyName("abilities")] // Maps JSON "abilities" to C# "AbilityIds"
    public List<string> AbilityIds { get; set; } = new();
}