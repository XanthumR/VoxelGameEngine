#pragma once

#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>

// Which game object stands on each world column (x, z). Answers "is this footprint free?" for
// placement and "which building is here?" for demolishing and selection.
class OccupancyGrid {
public:
    OccupancyGrid();

    // INVALID_GAME_OBJECT for a free column
    GameObjectId At(int wx, int wz) const;

    // True when every column of the rectangle (min corner, size in columns) is free
    bool IsFree(glm::ivec2 minColumn, glm::ivec2 size) const;

    // Claims the rectangle for the object; does nothing and returns false if any column is taken
    bool Occupy(glm::ivec2 minColumn, glm::ivec2 size, GameObjectId id);

    // Frees the columns of the rectangle that this object holds
    void Release(glm::ivec2 minColumn, glm::ivec2 size, GameObjectId id);

    size_t OccupiedColumns() const { return m_Columns.size(); }

private:
    static uint64_t ColumnKey(int wx, int wz) { return ((uint64_t)(uint32_t)wx << 32) | (uint32_t)wz; }

    std::unordered_map<uint64_t, GameObjectId> m_Columns;
};
