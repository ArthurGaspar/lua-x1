function OnCast(caster, targetPos)
    SpawnProjectile {
        caster = caster,
        speed = 14,
        radius = 0.15,
        damage = 55,
        target = targetPos,
        fx = "fx/spark_bolt"
    }
    print("[Lua] Spark Bolt fired")
    return true
end
