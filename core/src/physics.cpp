#include "physics.h"
#include <algorithm>

int32_t SpatialGrid::GetCellIndex(int32_t fixed_x, int32_t fixed_y) const {
    int32_t cx = fixed_x / CELL_SIZE;
    int32_t cy = fixed_y / CELL_SIZE;

    // prevent memory access out of bondaries of the map
    cx = std::max(0, std::min(cx, MAP_WIDTH_CELLS - 1));
    cy = std::max(0, std::min(cy, MAP_HEIGHT_CELLS - 1));

    // 2d grid --> 1 dimension array
    return cy * MAP_WIDTH_CELLS + cx;
}

void SpatialGrid::Clear() {
    for (auto& cell : cells) {
        cell.clear();
    }
}

void SpatialGrid::Insert(const EntityState& entity) {
    // for now insert the entity's center point into the grid. this means very large entities might not be registered in all occupied cells
    int32_t index = GetCellIndex(entity.pos_x, entity.pos_y);
    cells[index].push_back(entity.id);
}

std::vector<uint32_t> SpatialGrid::QueryRadius(int32_t center_x, int32_t center_y, int32_t radius) const {
    std::vector<uint32_t> found_entities;

    // broad-phase collision attempt
    int32_t min_x = center_x - radius;
    int32_t max_x = center_x + radius;
    int32_t min_y = center_y - radius;
    int32_t max_y = center_y + radius;

    int32_t start_cx = std::max(0, min_x / CELL_SIZE);
    int32_t end_cx   = std::min(MAP_WIDTH_CELLS - 1, max_x / CELL_SIZE);
    int32_t start_cy = std::max(0, min_y / CELL_SIZE);
    int32_t end_cy   = std::min(MAP_HEIGHT_CELLS - 1, max_y / CELL_SIZE);

    for (int32_t cy = start_cy; cy <= end_cy; ++cy) {
        for (int32_t cx = start_cx; cx <= end_cx; ++cx) {
            int32_t index = cy * MAP_WIDTH_CELLS + cx;
            for (uint32_t id : cells[index]) {
                found_entities.push_back(id);
            }
        }
    }

    // deterministic sort by ID so abilities always affect targets in the exact same order
    std::sort(found_entities.begin(), found_entities.end());
    
    // remove duplicate IDs
    found_entities.erase(std::unique(found_entities.begin(), found_entities.end()), found_entities.end());

    return found_entities;
}

bool SpatialGrid::CheckCollision(const EntityState& a, const EntityState& b) {
    int64_t dx = static_cast<int64_t>(a.pos_x) - b.pos_x;
    int64_t dy = static_cast<int64_t>(a.pos_y) - b.pos_y;
    int64_t dist_sq = (dx * dx) + (dy * dy);
    
    int64_t radius_sum = static_cast<int64_t>(a.radius) + b.radius;
    int64_t radius_sq = radius_sum * radius_sum;
    
    return dist_sq <= radius_sq;
}