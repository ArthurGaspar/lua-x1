function OnCast(caster)
    print("[Lua] OVERLOAD!")

    ApplyBuff(caster, "OverloadBuff", {duration = 8, power = 2.0})

    PlayFX("fx/overload_start", caster.position)
    ShakeCamera(0.6)

    return true
end
