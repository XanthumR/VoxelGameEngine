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
//  - each producer's cart: it sets out with the output when there are CART_CAPACITY goods (or
//    something to carry after CART_MAX_WAIT_TICKS, or at once when an input ran out that the
//    island has), drives downhill on the warehouse road distance to the warehouse, unloads into
//    island storage (waiting while it is full), loads the inputs the producer lacks and drives back
//    the same way. A cut road ahead turns it around; with the road home cut it is home at once.
//  - a paused producer (the player's choice) does nothing and frees its jobs; its cart still runs
//  - each producer's productivity history: the average of every 30 s, the last 10 minutes
// Integer math only, so a run is reproducible.
class ProductionSystem {
public:
    void Update(GameObjectRegistry& objects, IslandEconomyManager& economy, IslandRegistry& islands, const OccupancyGrid& occupancy,
        const RoadNetwork& roads, TreeRegistry& trees, uint32_t worldRevision, uint64_t tick);

    uint32_t Working() const { return m_Working; }
    uint32_t Producers() const { return m_Producers; }
    uint32_t CartsOnRoad() const { return m_CartsOnRoad; }

    // The cart route from the road tile next to a footprint with the lowest warehouse distance,
    // downhill to a tile touching a warehouse. False when no road tile around it is in reach.
    static bool FindCartPath(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, ProductionComponent& production);
    // The cart is home at once with what it carries (its producer moved)
    static void RecallCart(ProductionComponent& production);
    // Adds this tick's productivity to the history (a sample every PRODUCTIVITY_SAMPLE_TICKS)
    static void SampleProductivity(ProductionComponent& production);

private:
    void UpdateLocations(GameObjectRegistry& objects, IslandRegistry& islands, const OccupancyGrid& occupancy, const RoadNetwork& roads, TreeRegistry& trees);
    void UpdateWorkforce(const GameObjectRegistry& objects, IslandEconomyManager& economy);
    void Produce(GameObjectRegistry& objects, IslandEconomyManager& economy, TreeRegistry& trees, GameObjectId id, uint64_t tick);
    void UpdateCart(GameObjectRegistry& objects, IslandEconomyManager& economy, const RoadNetwork& roads, GameObjectId id);

    uint32_t m_LastWorldRevision = 0xFFFFFFFF;
    uint32_t m_Working = 0;
    uint32_t m_Producers = 0;
    uint32_t m_CartsOnRoad = 0;
};
