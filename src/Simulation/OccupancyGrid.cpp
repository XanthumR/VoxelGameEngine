#include "Simulation/OccupancyGrid.h"

OccupancyGrid::OccupancyGrid() {
    m_Columns.reserve(GameObjectRegistry::MAX_OBJECTS * 16); // About a 4 x 4 footprint per object
}

GameObjectId OccupancyGrid::At(int wx, int wz) const {
    auto it = m_Columns.find(ColumnKey(wx, wz));
    return it == m_Columns.end() ? INVALID_GAME_OBJECT : it->second;
}

bool OccupancyGrid::IsFree(glm::ivec2 minColumn, glm::ivec2 size) const {
    for (int z = 0; z < size.y; z++) {
        for (int x = 0; x < size.x; x++) {
            if (m_Columns.count(ColumnKey(minColumn.x + x, minColumn.y + z))) return false;
        }
    }
    return true;
}

bool OccupancyGrid::Occupy(glm::ivec2 minColumn, glm::ivec2 size, GameObjectId id) {
    if (id == INVALID_GAME_OBJECT || !IsFree(minColumn, size)) return false;
    for (int z = 0; z < size.y; z++) {
        for (int x = 0; x < size.x; x++) {
            m_Columns[ColumnKey(minColumn.x + x, minColumn.y + z)] = id;
        }
    }
    return true;
}

void OccupancyGrid::Release(glm::ivec2 minColumn, glm::ivec2 size, GameObjectId id) {
    for (int z = 0; z < size.y; z++) {
        for (int x = 0; x < size.x; x++) {
            auto it = m_Columns.find(ColumnKey(minColumn.x + x, minColumn.y + z));
            if (it != m_Columns.end() && it->second == id) m_Columns.erase(it);
        }
    }
}
