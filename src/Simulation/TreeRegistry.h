#pragma once

#include "World/FelledTrees.h"
#include "World/Foliage.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

class OccupancyGrid;
class RoadNetwork;
class SaveReader;
class SaveWriter;
class TerrainGenerator;

// A tree cut down or grown back, for the frame loop to update the voxels
struct TreeChange {
    glm::ivec2 root;
    bool felled; // false: it grew back
};

// The foliage of the world as the simulation sees it: trees, and the bushes, rocks, logs and
// flowers round them (World/Foliage.h). Where they stand comes from the terrain generator
// (TerrainGenerator::ForEachFoliageRoot), so only what was cut down or cleared is stored, with the
// tick it grows back. Nothing grows back while a building or road covers its spot. Lumberjacks
// see only the trees (ForEachStanding); buildings and roads clear everything (FellOver).
class TreeRegistry {
public:
    static constexpr int REGROW_TICKS = 3000;      // 5 minutes
    static constexpr int REGROW_RETRY_TICKS = 600; // Spot covered: try again a minute later

    explicit TreeRegistry(TerrainGenerator& terrain);

    // Standing trees with their root in the column box (inclusive)
    int CountStanding(glm::ivec2 minColumn, glm::ivec2 maxColumn);
    template <typename Function>
    void ForEachStanding(glm::ivec2 minColumn, glm::ivec2 maxColumn, Function function) {
        ForEachStandingFoliage(minColumn, maxColumn, [&](glm::ivec2 root, int model) {
            if (IsTreeModel(model)) function(root);
        });
    }
    // Standing foliage of every kind: function(root, model)
    template <typename Function>
    void ForEachStandingFoliage(glm::ivec2 minColumn, glm::ivec2 maxColumn, Function function);

    // Fells the standing tree nearest to center within the box; false if there is none
    bool FellNearest(glm::ivec2 center, glm::ivec2 minColumn, glm::ivec2 maxColumn, uint64_t tick);
    void Fell(glm::ivec2 root, uint64_t tick);
    // The standing foliage whose models reach over the column box (inclusive): a building or road
    // placed there clears it, as in Anno
    bool AnyTreeOver(glm::ivec2 minColumn, glm::ivec2 maxColumn);
    void FellOver(glm::ivec2 minColumn, glm::ivec2 maxColumn, uint64_t tick);
    bool IsFelled(glm::ivec2 root) const { return m_Felled.count(FelledTrees::Key(root)) != 0; }

    // Grows back the trees whose time has come (once per tick)
    void Update(uint64_t tick, const OccupancyGrid& occupancy, const RoadNetwork& roads);

    const std::vector<TreeChange>& Changes() const { return m_Changes; }
    void ClearChanges() { m_Changes.clear(); }
    uint32_t Revision() const { return m_Revision; } // Bumped whenever a tree is felled or grows back

    // Shared with the chunk workers so newly generated chunks leave felled trees out
    FelledTrees& Felled() { return m_FelledSet; }
    size_t FelledCount() const { return m_Felled.size(); }
    void Save(SaveWriter& out) const;
    void Load(SaveReader& in); // Replaces the felled trees (the world's chunks must be made again)

private:
    struct Root {
        glm::ivec2 column;
        int model; // FoliageLibrary model
    };
    // Foliage roots of one 32 x 32 chunk column, found once and cached
    const std::vector<Root>& RootsInChunkColumn(int cx, int cz);
    bool SpotCovered(glm::ivec2 root, const OccupancyGrid& occupancy, const RoadNetwork& roads) const;

    TerrainGenerator& m_Terrain;
    std::unordered_map<uint64_t, std::vector<Root>> m_RootCache;
    std::unordered_map<uint64_t, uint64_t> m_Felled; // Root key -> tick it grows back
    FelledTrees m_FelledSet;
    std::vector<TreeChange> m_Changes;
    std::vector<uint64_t> m_Regrown; // Scratch for Update
    uint32_t m_Revision = 0;
};

template <typename Function>
void TreeRegistry::ForEachStandingFoliage(glm::ivec2 minColumn, glm::ivec2 maxColumn, Function function) {
    for (int cz = minColumn.y >> 5; cz <= maxColumn.y >> 5; cz++) {
        for (int cx = minColumn.x >> 5; cx <= maxColumn.x >> 5; cx++) {
            for (const Root& root : RootsInChunkColumn(cx, cz)) {
                glm::ivec2 c = root.column;
                if (c.x < minColumn.x || c.y < minColumn.y || c.x > maxColumn.x || c.y > maxColumn.y) continue;
                if (!IsFelled(c)) function(c, root.model);
            }
        }
    }
}
