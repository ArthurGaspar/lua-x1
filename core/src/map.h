#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include "config.h"
#include "entity_state.h"

struct Waypoint {
    int32_t x;
    int32_t y;
};

struct Lane {
    std::vector<Waypoint> waypoints;
};

struct SpawnPoint {
    int32_t x, y;
    uint8_t party; // team
};

class Map {
public:
    Map();
    bool loadFromJSON(const std::string& path);

    int32_t width;
    int32_t height;

    std::vector<std::vector<bool>> walkable;
    int32_t gridWidth, gridHeight;

    std::vector<Lane> lanes;
    std::vector<SpawnPoint> spawnPoints;

    inline int32_t worldToGridX(int32_t worldX) const { return worldX / POS_SCALE; }
    inline int32_t worldToGridY(int32_t worldY) const { return worldY / POS_SCALE; }
    inline int32_t gridToWorldX(int32_t gridX) const { return gridX * POS_SCALE; }
    inline int32_t gridToWorldY(int32_t gridY) const { return gridY * POS_SCALE; }

    bool isWalkable(int32_t worldX, int32_t worldY) const;
    bool isWalkableGrid(int32_t gridX, int32_t gridY) const;
};