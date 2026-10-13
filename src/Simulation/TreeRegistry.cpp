#include "Simulation/TreeRegistry.h"

#include "Simulation/BuildingTypes.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/RoadNetwork.h"
#include "Simulation/SaveGame.h"
#include "World/TerrainGenerator.h"
#include "World/VoxModel.h"

TreeRegistry::TreeRegistry(TerrainGenerator& terrain) : m_Terrain(terrain) {
    m_RootCache.reserve(4096);
    m_Felled.reserve(4096);
    m_Changes.reserve(4096); // A big building can clear a forest at once
    m_Regrown.reserve(256);
}

const std::vector<TreeRegistry::Root>& TreeRegistry::RootsInChunkColumn(int cx, int cz) {
    uint64_t key = ((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz;
    auto it = m_RootCache.find(key);
    if (it != m_RootCache.end()) return it->second;
    std::vector<Root>& roots = m_RootCache[key]; // Allocates the first time a column is looked at
    m_Terrain.ForEachFoliageRoot(glm::ivec2(cx, cz) * 32, glm::ivec2(cx, cz) * 32 + 31, [&](glm::ivec2 root, int model) { roots.push_back({ root, model }); });
    return roots;
}

int TreeRegistry::CountStanding(glm::ivec2 minColumn, glm::ivec2 maxColumn) {
    int count = 0;
    ForEachStanding(minColumn, maxColumn, [&](glm::ivec2) { count++; });
    return count;
}

void TreeRegistry::Fell(glm::ivec2 root, uint64_t tick) {
    if (IsFelled(root)) return;
    m_Felled[FelledTrees::Key(root)] = tick + REGROW_TICKS;
    m_FelledSet.Add(root);
    if (m_Changes.size() < m_Changes.capacity()) m_Changes.push_back({ root, true });
    m_Revision++;
}

namespace {

template <typename Function>
void ForEachTreeOver(TreeRegistry& trees, TerrainGenerator& terrain, glm::ivec2 minColumn, glm::ivec2 maxColumn, Function function) {
    int reach = terrain.FoliageReach();
    trees.ForEachStandingFoliage(minColumn - reach, maxColumn + reach, [&](glm::ivec2 root, int index) {
        const VoxModel& model = terrain.FoliageModel(index);
        if (root.x + model.max.x >= minColumn.x && root.x + model.min.x <= maxColumn.x && root.y + model.max.z >= minColumn.y &&
            root.y + model.min.z <= maxColumn.y) {
            function(root);
        }
    });
}

} // namespace

bool TreeRegistry::AnyTreeOver(glm::ivec2 minColumn, glm::ivec2 maxColumn) {
    bool any = false;
    ForEachTreeOver(*this, m_Terrain, minColumn, maxColumn, [&](glm::ivec2) { any = true; });
    return any;
}

void TreeRegistry::FellOver(glm::ivec2 minColumn, glm::ivec2 maxColumn, uint64_t tick) {
    ForEachTreeOver(*this, m_Terrain, minColumn, maxColumn, [&](glm::ivec2 root) { Fell(root, tick); });
}

bool TreeRegistry::FellNearest(glm::ivec2 center, glm::ivec2 minColumn, glm::ivec2 maxColumn, uint64_t tick) {
    bool found = false;
    glm::ivec2 nearest(0);
    int64_t nearestDistance = 0;
    ForEachStanding(minColumn, maxColumn, [&](glm::ivec2 root) {
        glm::ivec2 d = root - center;
        int64_t distance = (int64_t)d.x * d.x + (int64_t)d.y * d.y;
        // Ties: the lower x, then z, so the choice does not depend on the cache order
        if (!found || distance < nearestDistance || (distance == nearestDistance && (root.x < nearest.x || (root.x == nearest.x && root.y < nearest.y)))) {
            found = true;
            nearest = root;
            nearestDistance = distance;
        }
    });
    if (found) Fell(nearest, tick);
    return found;
}

// A building or road on any tile the grown model would cover
bool TreeRegistry::SpotCovered(glm::ivec2 root, const OccupancyGrid& occupancy, const RoadNetwork& roads) const {
    int index = m_Terrain.FoliageAt(root.x, root.y);
    if (index < 0) return false;
    const VoxModel& model = m_Terrain.FoliageModel(index);
    glm::ivec2 minTile(ColumnToTile(root.x + model.min.x), ColumnToTile(root.y + model.min.z));
    glm::ivec2 maxTile(ColumnToTile(root.x + model.max.x), ColumnToTile(root.y + model.max.z));
    for (int tz = minTile.y; tz <= maxTile.y; tz++) {
        for (int tx = minTile.x; tx <= maxTile.x; tx++) {
            if (occupancy.At(glm::ivec2(tx, tz)) != INVALID_GAME_OBJECT || roads.IsRoad(glm::ivec2(tx, tz))) return true;
        }
    }
    return false;
}

void TreeRegistry::Update(uint64_t tick, const OccupancyGrid& occupancy, const RoadNetwork& roads) {
    m_Regrown.clear();
    for (auto& entry : m_Felled) {
        if (entry.second > tick) continue;
        glm::ivec2 root((int32_t)(uint32_t)(entry.first >> 32), (int32_t)(uint32_t)entry.first);
        if (SpotCovered(root, occupancy, roads)) {
            entry.second = tick + REGROW_RETRY_TICKS;
            continue;
        }
        if (m_Regrown.size() < m_Regrown.capacity()) m_Regrown.push_back(entry.first);
    }
    for (uint64_t key : m_Regrown) {
        glm::ivec2 root((int32_t)(uint32_t)(key >> 32), (int32_t)(uint32_t)key);
        m_Felled.erase(key);
        m_FelledSet.Remove(root);
        if (m_Changes.size() < m_Changes.capacity()) m_Changes.push_back({ root, false });
        m_Revision++;
    }
}

void TreeRegistry::Save(SaveWriter& out) const {
    out.Write((uint64_t)m_Felled.size());
    for (const auto& entry : m_Felled) {
        out.Write(entry.first);
        out.Write(entry.second);
    }
}

void TreeRegistry::Load(SaveReader& in) {
    m_Felled.clear();
    m_FelledSet.Clear();
    m_Changes.clear();
    uint64_t count = in.Read<uint64_t>();
    if (count > 1u << 24) in.Fail();
    for (uint64_t i = 0; i < count && in.Ok(); i++) {
        uint64_t key = in.Read<uint64_t>();
        uint64_t regrow = in.Read<uint64_t>();
        m_Felled[key] = regrow;
        m_FelledSet.Add(glm::ivec2((int32_t)(uint32_t)(key >> 32), (int32_t)(uint32_t)key));
    }
    m_Revision++;
}
