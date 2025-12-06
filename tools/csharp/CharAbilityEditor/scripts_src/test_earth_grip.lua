function OnCast(caster, targetPos)
    RootUnitsInRadius(targetPos, 2.5, 2.0)
    print("[Lua] Earth Grip used")
    return true
end
