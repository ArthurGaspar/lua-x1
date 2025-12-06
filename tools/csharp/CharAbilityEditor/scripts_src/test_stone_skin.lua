function OnCast(caster)
    ApplyBuff(caster, "StoneSkin", {duration = 10, armor = 40})
    print("[Lua] Stone Skin applied")
    return true
end
