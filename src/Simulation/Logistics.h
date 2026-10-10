#pragma once

#include "Simulation/GameObjects.h"
#include "Simulation/RoadNetwork.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

constexpr int WAREHOUSE_ROAD_RANGE = 30; // Road tiles a warehouse reaches (120 voxels)
constexpr int MARKET_ROAD_RANGE = SERVICE_ROAD_RANGE[(size_t)ServiceType::Marketplace]; // Road tiles a marketplace reaches (80 voxels)

// The best reached road tile touching a footprint
struct FootprintConnection {
    bool connected = false;
    GameObjectId source = INVALID_GAME_OBJECT; // The warehouse or service building
    uint16_t roadDistance = 0xFFFF;
};

// Reach along roads, and which buildings it connects. A warehouse reaches the road tiles touching
// its footprint (distance 1) and spreads along the road up to WAREHOUSE_ROAD_RANGE; a building is
// connected when a reached road tile touches it. Service buildings (marketplaces, schools, ...) that
// are connected spread the same way (SERVICE_ROAD_RANGE of their type) and serve the buildings
// their reach touches. Recomputed from scratch (a multi-source breadth-first search per kind)
// whenever roads or buildings change.
class LogisticsSystem {
public:
    LogisticsSystem();

    // Rebuilds when the road network or the buildings changed since the last call; true if it did
    bool Update(GameObjectRegistry& objects, RoadNetwork& roads, uint32_t buildingsRevision);
    void Rebuild(GameObjectRegistry& objects, RoadNetwork& roads);

    // Use the road distances of the last rebuild
    static FootprintConnection ConnectionOf(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles);
    static FootprintConnection ServiceConnectionOf(const RoadNetwork& roads, ServiceType service, glm::ivec2 minTile, glm::ivec2 tiles);

    // The road tiles a warehouse or service building with this footprint would reach (placement
    // preview). Does not allocate when out has capacity for the result.
    void PreviewReach(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, int range, std::vector<glm::ivec2>& out);

    uint32_t Revision() const { return m_Revision; } // Bumped on every rebuild
    uint32_t ConnectedBuildings() const { return m_ConnectedBuildings; }
    uint32_t TotalBuildings() const { return m_TotalBuildings; }

private:
    struct QueueEntry {
        glm::ivec2 tile;
        uint16_t distance;
    };

    // Seeds the road tiles around a footprint (distance 1) for one kind of reach (WAREHOUSE_REACH or a ServiceType)
    void Seed(RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, GameObjectId source, int kind);
    // Breadth-first from the seeded queue; the first source to reach a tile owns it
    void Spread(RoadNetwork& roads, int range, int kind);

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
