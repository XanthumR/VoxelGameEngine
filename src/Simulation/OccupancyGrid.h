#pragma once

#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>

// Which game object stands on each build tile (x, z; see TILE_SIZE in BuildingTypes.h). Answers
// "is this footprint free?" for placement and "which building is here?" for demolishing.
class OccupancyGrid {
public:
    OccupancyGrid();

    // INVALID_GAME_OBJECT for a free tile
    GameObjectId At(glm::ivec2 tile) const;

    // True when every tile of the rectangle (min corner, size in tiles) is free
    bool IsFree(glm::ivec2 minTile, glm::ivec2 size) const;

    // Claims the rectangle for the object; does nothing and returns false if any tile is taken
    bool Occupy(glm::ivec2 minTile, glm::ivec2 size, GameObjectId id);

    // Frees the tiles of the rectangle that this object holds
    void Release(glm::ivec2 minTile, glm::ivec2 size, GameObjectId id);

    size_t OccupiedTiles() const { return m_Tiles.size(); }

private:
    static uint64_t TileKey(int tx, int tz) { return ((uint64_t)(uint32_t)tx << 32) | (uint32_t)tz; }

    std::unordered_map<uint64_t, GameObjectId> m_Tiles;
};
