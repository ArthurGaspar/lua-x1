// map.cpp
#include <fstream>
#include <iostream>
#include <vector>
#include "map.h"
#include "../../vendor/cpp/nlohmann/json.hpp"

using json = nlohmann::json;

Map::Map() : width(0), height(0), gridWidth(0), gridHeight(0) {}

bool Map::loadFromJSON(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Map] Failed to open file: " << path << std::endl;
        return false;
    }

    json data;
    try {
        data = json::parse(file);
    } catch (const json::parse_error& e) {
        std::cerr << "[Map] JSON parse error: " << e.what() << std::endl;
        return false;
    }

    // Load dimensions (width and height)
    if (data.contains("width") && data["width"].is_number()) {
        width = data["width"].get<int32_t>();
    } else {
        std::cerr << "[Map] Missing or invalid 'width' field" << std::endl;
        return false;
    }
    if (data.contains("height") && data["height"].is_number()) {
        height = data["height"].get<int32_t>();
    } else {
        std::cerr << "[Map] Missing or invalid 'height' field" << std::endl;
        return false;
    }

    gridWidth = width;
    gridHeight = height;
    
    walkable.resize(gridHeight, std::vector<bool>(gridWidth, false));

    // Load walkable grid
    if (data.contains("walkable") && data["walkable"].is_array()) {
        auto walkableArray = data["walkable"];
        
        // Check if it's a 2D array
        if (walkableArray.size() != static_cast<size_t>(gridHeight)) {
            std::cerr << "[Map] Walkable grid height mismatch. Expected: " << gridHeight 
                      << ", Got: " << walkableArray.size() << std::endl;
            return false;
        }

        for (int32_t y = 0; y < gridHeight; ++y) {
            if (!walkableArray[y].is_array()) {
                std::cerr << "[Map] Walkable row " << y << " is not an array" << std::endl;
                return false;
            }
            
            auto row = walkableArray[y];
            if (row.size() != static_cast<size_t>(gridWidth)) {
                std::cerr << "[Map] Walkable row " << y << " width mismatch. Expected: " 
                          << gridWidth << ", Got: " << row.size() << std::endl;
                return false;
            }
            
            for (int32_t x = 0; x < gridWidth; ++x) {
                // JSON values are booleans 0 / 1
                int val = row[x].get<int>();
                walkable[y][x] = (val != 0);
            }
        }
    } else {
        std::cerr << "[Map] No walkable grid provided, assuming all cells walkable" << std::endl;
        for (int32_t y = 0; y < gridHeight; ++y) {
            for (int32_t x = 0; x < gridWidth; ++x) {
                walkable[y][x] = true;
            }
        }
    }

    // Load lanes
    if (data.contains("lanes") && data["lanes"].is_array()) {
        auto lanesArray = data["lanes"];
        
        for (const auto& laneJson : lanesArray) {
            Lane lane;
            
            if (laneJson.contains("waypoints") && laneJson["waypoints"].is_array()) {
                auto waypointsArray = laneJson["waypoints"];
                
                for (const auto& wpJson : waypointsArray) {
                    Waypoint wp;
                    
                    if (wpJson.contains("x") && wpJson["x"].is_number()) {
                        wp.x = wpJson["x"].get<int32_t>() * POS_SCALE;
                    } else {
                        std::cerr << "[Map] Waypoint missing x coordinate" << std::endl;
                        continue;
                    }
                    
                    if (wpJson.contains("y") && wpJson["y"].is_number()) {
                        wp.y = wpJson["y"].get<int32_t>() * POS_SCALE;
                    } else {
                        std::cerr << "[Map] Waypoint missing y coordinate" << std::endl;
                        continue;
                    }
                    
                    lane.waypoints.push_back(wp);
                }
            }
            
            lanes.push_back(lane);
        }
    } else {
        std::cerr << "[Map] No lanes defined" << std::endl;
    }

    // Load spawn points
    if (data.contains("spawnPoints") && data["spawnPoints"].is_array()) {
        auto spawnsArray = data["spawnPoints"];
        
        for (const auto& spawnJson : spawnsArray) {
            SpawnPoint spawn;
            
            if (spawnJson.contains("x") && spawnJson["x"].is_number()) {
                spawn.x = spawnJson["x"].get<int32_t>() * POS_SCALE;
            } else {
                std::cerr << "[Map] Spawn point missing x coordinate" << std::endl;
                continue;
            }
            
            if (spawnJson.contains("y") && spawnJson["y"].is_number()) {
                spawn.y = spawnJson["y"].get<int32_t>() * POS_SCALE;
            } else {
                std::cerr << "[Map] Spawn point missing y coordinate" << std::endl;
                continue;
            }
            
            if (spawnJson.contains("party") && spawnJson["party"].is_number()) {
                spawn.party = spawnJson["party"].get<uint8_t>();
            } else {
                std::cerr << "[Map] Spawn point missing party" << std::endl;
                spawn.party = 0;
            }
            
            spawnPoints.push_back(spawn);
        }
    } else {
        std::cerr << "[Map] No spawn points defined" << std::endl;
    }

    std::cout << "[Map] Successfully loaded map: " << width << "x" << height 
              << " cells, " << lanes.size() << " lanes, " 
              << spawnPoints.size() << " spawn points" << std::endl;
    
    return true;
}

bool Map::isWalkable(int32_t worldX, int32_t worldY) const {
    int32_t gridX = worldToGridX(worldX);
    int32_t gridY = worldToGridY(worldY);
    return isWalkableGrid(gridX, gridY);
}

bool Map::isWalkableGrid(int32_t gridX, int32_t gridY) const {
    if (gridX < 0 || gridX >= gridWidth || gridY < 0 || gridY >= gridHeight) {
        return false; // Out of bounds should not be walkable
    }
    return walkable[gridY][gridX];
}