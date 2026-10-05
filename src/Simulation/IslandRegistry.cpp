#include "Simulation/IslandRegistry.h"

#include "World/TerrainGenerator.h"
#include "World/WorldConstants.h"

#include <algorithm>

IslandRegistry::IslandRegistry(TerrainGenerator& terrain) : m_Terrain(terrain) {
    m_CellIsland.reserve(MAX_ISLAND_CELLS);
    m_Islands.reserve(256);
    m_FloodStack.reserve(MAX_ISLAND_CELLS);
}

uint64_t IslandRegistry::CellKey(glm::ivec2 cell) {
    return ((uint64_t)(uint32_t)cell.x << 32) | (uint32_t)cell.y;
}

bool IslandRegistry::IsLandColumn(int wx, int wz) {
    return m_Terrain.TerrainHeightAt(wx, wz) >= SEA_LEVEL + ISLAND_HEIGHT;
}

bool IslandRegistry::IsLandCell(glm::ivec2 cell) {
    return IsLandColumn(cell.x * CELL_SIZE + CELL_SIZE / 2, cell.y * CELL_SIZE + CELL_SIZE / 2);
}

IslandId IslandRegistry::IslandIdAt(int wx, int wz) {
    if (!IsLandColumn(wx, wz)) return NO_ISLAND;

    // Arithmetic shift floors negative coordinates too
    glm::ivec2 cell(wx >> 3, wz >> 3);
    static_assert(CELL_SIZE == 8, "Shift above assumes 8-column cells");

    auto found = m_CellIsland.find(CellKey(cell));
    if (found != m_CellIsland.end()) return found->second;
    if (IsLandCell(cell)) return FloodFill(cell);

    // A land column in a cell whose center is water (the coast): take a neighbouring land cell's island
    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dz == 0) continue;
            glm::ivec2 neighbour = cell + glm::ivec2(dx, dz);
            auto it = m_CellIsland.find(CellKey(neighbour));
            if (it != m_CellIsland.end()) return it->second;
            if (IsLandCell(neighbour)) return FloodFill(neighbour);
        }
    }
    return NO_ISLAND;
}

IslandId IslandRegistry::FloodFill(glm::ivec2 startCell) {
    IslandInfo info;
    info.id = (IslandId)m_Islands.size() + 1;
    info.minColumn = startCell * CELL_SIZE;
    info.maxColumn = startCell * CELL_SIZE + (CELL_SIZE - 1);

    m_FloodStack.clear();
    m_FloodStack.push_back(startCell);
    m_CellIsland[CellKey(startCell)] = info.id;

    const glm::ivec2 directions[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    while (!m_FloodStack.empty()) {
        glm::ivec2 cell = m_FloodStack.back();
        m_FloodStack.pop_back();
        info.cellCount++;
        info.minColumn = glm::min(info.minColumn, cell * CELL_SIZE);
        info.maxColumn = glm::max(info.maxColumn, cell * CELL_SIZE + (CELL_SIZE - 1));

        for (const glm::ivec2& direction : directions) {
            glm::ivec2 next = cell + direction;
            uint64_t key = CellKey(next);
            if (m_CellIsland.count(key) || !IsLandCell(next)) continue;
            if (info.cellCount + (int)m_FloodStack.size() >= MAX_ISLAND_CELLS) {
                info.truncated = true;
                continue;
            }
            m_CellIsland[key] = info.id; // Claimed when queued, so no cell is queued twice
            m_FloodStack.push_back(next);
        }
    }

    m_Islands.push_back(info);
    return info.id;
}

const IslandInfo* IslandRegistry::Info(IslandId id) const {
    if (id == NO_ISLAND || id > m_Islands.size()) return nullptr;
    return &m_Islands[id - 1];
}
