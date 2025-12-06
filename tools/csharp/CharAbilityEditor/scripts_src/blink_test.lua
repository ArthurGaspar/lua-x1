function OnCast(caster, targetPos)
    print("[Lua] Blink cast!")

    TeleportUnit(caster, targetPos)
    PlayFX("fx/blink_cast", caster.position)

    return true
end
