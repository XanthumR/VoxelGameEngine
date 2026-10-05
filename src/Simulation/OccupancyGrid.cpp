#include "Simulation/OccupancyGrid.h"

OccupancyGrid::OccupancyGrid() {
    m_Tiles.reserve(GameObjectRegistry::MAX_OBJECTS * 16); // About a 4 x 4 tile footprint per object
}

GameObjectId OccupancyGrid::At(glm::ivec2 tile) const {
    auto it = m_Tiles.find(TileKey(tile.x, tile.y));
    return it == m_Tiles.end() ? INVALID_GAME_OBJECT : it->second;
}

bool OccupancyGrid::IsFree(glm::ivec2 minTile, glm::ivec2 size) const {
    for (int z = 0; z < size.y; z++) {
        for (int x = 0; x < size.x; x++) {
            if (m_Tiles.count(TileKey(minTile.x + x, minTile.y + z))) return false;
        }
    }
    return true;
}

bool OccupancyGrid::Occupy(glm::ivec2 minTile, glm::ivec2 size, GameObjectId id) {
    if (id == INVALID_GAME_OBJECT || !IsFree(minTile, size)) return false;
    for (int z = 0; z < size.y; z++) {
        for (int x = 0; x < size.x; x++) {
            m_Tiles[TileKey(minTile.x + x, minTile.y + z)] = id;
        }
    }
    return true;
}

void OccupancyGrid::Release(glm::ivec2 minTile, glm::ivec2 size, GameObjectId id) {
    for (int z = 0; z < size.y; z++) {
        for (int x = 0; x < size.x; x++) {
            auto it = m_Tiles.find(TileKey(minTile.x + x, minTile.y + z));
            if (it != m_Tiles.end() && it->second == id) m_Tiles.erase(it);
        }
    }
}
