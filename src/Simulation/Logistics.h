#pragma once

#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

class RoadNetwork;

constexpr int WAREHOUSE_ROAD_RANGE = 30; // Road tiles a warehouse reaches (120 voxels)

// Where a footprint would connect: the best in-range road tile touching it
struct FootprintConnection {
    bool connected = false;
    GameObjectId warehouse = INVALID_GAME_OBJECT;
    uint16_t roadDistance = 0xFFFF;
};

// Warehouse reach along roads, and which buildings it connects. A warehouse reaches the road tiles
// touching its footprint (distance 1) and spreads along the road up to WAREHOUSE_ROAD_RANGE. A
// building is connected when a road tile touching it is reached. Recomputed from scratch (a
// multi-source breadth-first search) whenever roads or buildings change.
class LogisticsSystem {
public:
    LogisticsSystem();

    // Rebuilds when the road network or the buildings changed since the last call; true if it did
    bool Update(GameObjectRegistry& objects, RoadNetwork& roads, uint32_t buildingsRevision);
    void Rebuild(GameObjectRegistry& objects, RoadNetwork& roads);

    // Uses the road distances of the last rebuild
    static FootprintConnection ConnectionOf(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles);

    // The road tiles a warehouse with this footprint would reach (placement preview). Does not
    // allocate when out has capacity for the result.
    void PreviewReach(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, std::vector<glm::ivec2>& out);

    uint32_t Revision() const { return m_Revision; } // Bumped on every rebuild
    uint32_t ConnectedBuildings() const { return m_ConnectedBuildings; }
    uint32_t TotalBuildings() const { return m_TotalBuildings; }

private:
    struct QueueEntry {
        glm::ivec2 tile;
        uint16_t distance;
    };

    // Preview search area: every tile within range of a footprint of up to MAX_PREVIEW_FOOTPRINT tiles
    static constexpr int MAX_PREVIEW_FOOTPRINT = 8;
    static constexpr int PREVIEW_SIDE = 2 * (WAREHOUSE_ROAD_RANGE + 1) + MAX_PREVIEW_FOOTPRINT;

    std::vector<QueueEntry> m_Queue;
    std::array<uint16_t, PREVIEW_SIDE * PREVIEW_SIDE> m_PreviewDistance = {};
    uint32_t m_LastRoadRevision = 0xFFFFFFFF;
    uint32_t m_LastBuildingsRevision = 0xFFFFFFFF;
    uint32_t m_Revision = 0;
    uint32_t m_ConnectedBuildings = 0;
    uint32_t m_TotalBuildings = 0;
};

// Calls function(tile) for each tile 4-adjacent to the outside of a footprint
template <typename Function>
void ForEachTileAround(glm::ivec2 minTile, glm::ivec2 tiles, Function function) {
    for (int x = 0; x < tiles.x; x++) {
        function(glm::ivec2(minTile.x + x, minTile.y - 1));
        function(glm::ivec2(minTile.x + x, minTile.y + tiles.y));
    }
    for (int z = 0; z < tiles.y; z++) {
        function(glm::ivec2(minTile.x - 1, minTile.y + z));
        function(glm::ivec2(minTile.x + tiles.x, minTile.y + z));
    }
}
