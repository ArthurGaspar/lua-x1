function OnCast(caster)
    print("[Lua] Arcane Shield activated")
    ApplyBuff(caster, "ArcaneShield", {duration = 5, shield = 120})
    return true
end
