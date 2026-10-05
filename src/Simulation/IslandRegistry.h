#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

class TerrainGenerator;

using IslandId = uint32_t;
constexpr IslandId NO_ISLAND = 0; // Water, or a sliver of coast too small to belong to an island

struct IslandInfo {
    IslandId id = NO_ISLAND;
    int cellCount = 0;
    glm::ivec2 minColumn = glm::ivec2(0); // Bounding box in world columns (x, z), inclusive
    glm::ivec2 maxColumn = glm::ivec2(0);
    bool truncated = false; // The flood fill hit MAX_ISLAND_CELLS; the rest of the land is a new island
};

// Finds which island a world column belongs to. Works on a coarse grid of CELL_SIZE x CELL_SIZE
// columns: a cell is land when its center column is at island height. The first query on an
// unknown land cell flood-fills its whole island (4-connected) and gives it the next ID, so IDs
// depend on the order islands are discovered but stay stable afterwards. Only the terrain noise is
// sampled, never voxel data, so it works for islands that are not loaded. Discovering an island
// allocates; looking up a known one does not.
class IslandRegistry {
public:
    static constexpr int CELL_SIZE = 8;              // Columns per cell side
    static constexpr int MAX_ISLAND_CELLS = 16384;   // 1024 x 1024 columns

    explicit IslandRegistry(TerrainGenerator& terrain);

    // The island of a column; NO_ISLAND for water and beaches below island height
    IslandId IslandIdAt(int wx, int wz);

    // Info for an island ID returned by IslandIdAt; nullptr for NO_ISLAND
    const IslandInfo* Info(IslandId id) const;
    size_t IslandCount() const { return m_Islands.size(); }

    // True when the column is flat island ground (TerrainGenerator height check only)
    bool IsLandColumn(int wx, int wz);

private:
    static uint64_t CellKey(glm::ivec2 cell);
    bool IsLandCell(glm::ivec2 cell);
    IslandId FloodFill(glm::ivec2 startCell);

    TerrainGenerator& m_Terrain;
    std::unordered_map<uint64_t, IslandId> m_CellIsland; // Land cells only
    std::vector<IslandInfo> m_Islands;                   // m_Islands[id - 1]
    std::vector<glm::ivec2> m_FloodStack;                // Reserved once, reused by every fill
};
