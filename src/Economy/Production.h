#pragma once

#include "Simulation/GameObjects.h"

#include <cstdint>

class IslandEconomyManager;
class IslandRegistry;
class OccupancyGrid;
class RoadNetwork;
class TreeRegistry;

// The producers, one step per simulation tick (recipes in ProductionChains.h):
//  - workforce per island and tier: the jobs of the connected producers against the residents of
//    that tier; when there are fewer residents than jobs, every producer of the tier slows down by
//    the same share
//  - each producer's location factor (trees, pasture, coast), recomputed whenever buildings, roads
//    or trees change
//  - each producer's cycle: progress grows by its productivity (workforce share x location factor)
//    while it has its inputs and room for its output; a finished cycle uses one of each input and
//    makes one output. A lumberjack also cuts down the nearest tree.
// Integer math only, so a run is reproducible.
class ProductionSystem {
public:
    void Update(GameObjectRegistry& objects, IslandEconomyManager& economy, IslandRegistry& islands, const OccupancyGrid& occupancy,
        const RoadNetwork& roads, TreeRegistry& trees, uint32_t worldRevision, uint64_t tick);

    uint32_t Working() const { return m_Working; }
    uint32_t Producers() const { return m_Producers; }

private:
    void UpdateLocations(GameObjectRegistry& objects, IslandRegistry& islands, const OccupancyGrid& occupancy, const RoadNetwork& roads, TreeRegistry& trees);
    void UpdateWorkforce(const GameObjectRegistry& objects, IslandEconomyManager& economy);
    void Produce(GameObjectRegistry& objects, IslandEconomyManager& economy, TreeRegistry& trees, GameObjectId id, uint64_t tick);

    uint32_t m_LastWorldRevision = 0xFFFFFFFF;
    uint32_t m_Working = 0;
    uint32_t m_Producers = 0;
};
