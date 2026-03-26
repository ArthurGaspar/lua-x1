#pragma once
#include <vector>
#include <cstdint>
#include "entity_state.h"

// POS_SCALE = 1000. A cell size of 5.0 world units = 5000 fixed-point units.
constexpr int32_t CELL_SIZE = 5000; 
constexpr int32_t MAP_WIDTH_CELLS = 30; // 150x150 world units map
constexpr int32_t MAP_HEIGHT_CELLS = 30;

class SpatialGrid {
private:
    // Flat array representing the 2D grid.
    // Each cell holds a list of Entity IDs.
    std::vector<uint32_t> cells[MAP_WIDTH_CELLS * MAP_HEIGHT_CELLS];

    // Helper: fixed-point world coordinates to a 1D array index
    int32_t GetCellIndex(int32_t fixed_x, int32_t fixed_y) const;

public:
    void Clear();
    void Insert(const EntityState& entity);
    
    // broad-phase collision query -> entity IDs
    std::vector<uint32_t> QueryRadius(int32_t center_x, int32_t center_y, int32_t radius) const;
    
    // exact-phase collision check (Circle-Circle)
    static bool CheckCollision(const EntityState& a, const EntityState& b);
};