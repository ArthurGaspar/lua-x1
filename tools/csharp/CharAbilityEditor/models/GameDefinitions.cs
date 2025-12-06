namespace CharAbilityEditor;

public class GameDefinitions
{
    public Dictionary<string, CharDef> Characters { get; set; } = new();
    public Dictionary<string, AbilityDef> Abilities { get; set; } = new();
}