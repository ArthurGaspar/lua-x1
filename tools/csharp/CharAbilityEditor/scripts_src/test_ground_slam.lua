function OnCast(caster)
    print("[Lua] Ground Slam!")

    DealAOEDamage(caster.position, 3.0, 100)
    PlayFX("fx/ground_slam", caster.position)

    return true
end
