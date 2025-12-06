function OnCast(caster, targetPos)
    TeleportUnit(caster, targetPos)
    ApplyBuff(caster, "ShadowVeil", {duration = 3})
    print("[Lua] Shadow Step")
    return true
end
