#pragma once

#include "World/FelledTrees.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

class OccupancyGrid;
class RoadNetwork;
class TerrainGenerator;

// A tree cut down or grown back, for the frame loop to update the voxels
struct TreeChange {
    glm::ivec2 root;
    bool felled; // false: it grew back
};

// The trees of the world as the simulation sees them. Where trees stand comes from the terrain
// generator (TerrainGenerator::IsTreeRoot), so only the trees that were cut down are stored, with
// the tick they grow back. A tree does not grow back while a building or road covers its spot.
class TreeRegistry {
public:
    static constexpr int REGROW_TICKS = 3000;      // 5 minutes
    static constexpr int REGROW_RETRY_TICKS = 600; // Spot covered: try again a minute later

    explicit TreeRegistry(TerrainGenerator& terrain);

    // Standing trees with their root in the column box (inclusive)
    int CountStanding(glm::ivec2 minColumn, glm::ivec2 maxColumn);
    template <typename Function>
    void ForEachStanding(glm::ivec2 minColumn, glm::ivec2 maxColumn, Function function);

    // Fells the standing tree nearest to center within the box; false if there is none
    bool FellNearest(glm::ivec2 center, glm::ivec2 minColumn, glm::ivec2 maxColumn, uint64_t tick);
    void Fell(glm::ivec2 root, uint64_t tick);
    bool IsFelled(glm::ivec2 root) const { return m_Felled.count(FelledTrees::Key(root)) != 0; }

    // Grows back the trees whose time has come (once per tick)
    void Update(uint64_t tick, const OccupancyGrid& occupancy, const RoadNetwork& roads);

    const std::vector<TreeChange>& Changes() const { return m_Changes; }
    void ClearChanges() { m_Changes.clear(); }
    uint32_t Revision() const { return m_Revision; } // Bumped whenever a tree is felled or grows back

    // Shared with the chunk workers so newly generated chunks leave felled trees out
    FelledTrees& Felled() { return m_FelledSet; }
    size_t FelledCount() const { return m_Felled.size(); }

private:
    // Tree roots of one 32 x 32 chunk column, found once and cached
    const std::vector<glm::ivec2>& RootsInChunkColumn(int cx, int cz);
    bool SpotCovered(glm::ivec2 root, const OccupancyGrid& occupancy, const RoadNetwork& roads) const;

    TerrainGenerator& m_Terrain;
    std::unordered_map<uint64_t, std::vector<glm::ivec2>> m_RootCache;
    std::unordered_map<uint64_t, uint64_t> m_Felled; // Root key -> tick it grows back
    FelledTrees m_FelledSet;
    std::vector<TreeChange> m_Changes;
    std::vector<uint64_t> m_Regrown; // Scratch for Update
    uint32_t m_Revision = 0;
};

template <typename Function>
void TreeRegistry::ForEachStanding(glm::ivec2 minColumn, glm::ivec2 maxColumn, Function function) {
    for (int cz = minColumn.y >> 5; cz <= maxColumn.y >> 5; cz++) {
        for (int cx = minColumn.x >> 5; cx <= maxColumn.x >> 5; cx++) {
            for (const glm::ivec2& root : RootsInChunkColumn(cx, cz)) {
                if (root.x < minColumn.x || root.y < minColumn.y || root.x > maxColumn.x || root.y > maxColumn.y) continue;
                if (!IsFelled(root)) function(root);
            }
        }
    }
}
