#include "Economy/Production.h"

#include "Economy/IslandEconomy.h"
#include "Economy/ProductionChains.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/ProducerLocation.h"
#include "Simulation/TreeRegistry.h"

#include <algorithm>

namespace {

void FootprintOf(const VoxelAnchorComponent& anchor, glm::ivec2& minTile, glm::ivec2& tiles) {
    minTile = glm::ivec2(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
    tiles = anchor.footprint / TILE_SIZE;
}

bool IsProducer(const GameObjectRegistry& objects, GameObjectId id) {
    return BUILDING_TYPES[objects.Building(id).type].role == BuildingRole::Producer;
}

const ProductionChain& ChainOf(const GameObjectRegistry& objects, GameObjectId id) {
    return PRODUCTION_CHAINS[BUILDING_TYPES[objects.Building(id).type].chain];
}

} // namespace

void ProductionSystem::Update(GameObjectRegistry& objects, IslandEconomyManager& economy, IslandRegistry& islands, const OccupancyGrid& occupancy,
    const RoadNetwork& roads, TreeRegistry& trees, uint32_t worldRevision, uint64_t tick) {
    if (worldRevision != m_LastWorldRevision) {
        m_LastWorldRevision = worldRevision;
        UpdateLocations(objects, islands, occupancy, roads, trees);
    }
    UpdateWorkforce(objects, economy);

    m_Working = 0;
    m_Producers = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || !IsProducer(objects, id)) continue;
        Produce(objects, economy, trees, id, tick);
        m_Producers++;
        if (objects.Production(id).status == ProducerStatus::Working) m_Working++;
    }
}

void ProductionSystem::UpdateLocations(GameObjectRegistry& objects, IslandRegistry& islands, const OccupancyGrid& occupancy, const RoadNetwork& roads,
    TreeRegistry& trees) {
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || !IsProducer(objects, id)) continue;
        glm::ivec2 minTile, tiles;
        FootprintOf(objects.Anchor(id), minTile, tiles);
        LocationReport report = EvaluateLocation(ChainOf(objects, id), minTile, tiles, islands, occupancy, roads, trees);
        objects.Production(id).locationFactor = (int16_t)report.factor;
    }
}

void ProductionSystem::UpdateWorkforce(const GameObjectRegistry& objects, IslandEconomyManager& economy) {
    for (size_t i = 0; i < economy.IslandSlotCount(); i++) economy.IslandAt(i).jobs.fill(0);
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || !IsProducer(objects, id) || !objects.Logistics(id).connected) continue;
        IslandStorage* storage = economy.Find(objects.Building(id).island);
        if (!storage) continue;
        const ProductionChain& chain = ChainOf(objects, id);
        storage->jobs[chain.workforceTier] += chain.workforce;
    }
    for (size_t i = 0; i < economy.IslandSlotCount(); i++) {
        IslandStorage& storage = economy.IslandAt(i);
        for (int tier = 0; tier < TIER_COUNT; tier++) {
            int jobs = storage.jobs[tier];
            storage.workforce[tier] = jobs == 0 ? 1000 : std::min(1000, storage.population[tier] * 1000 / jobs);
        }
    }
}

void ProductionSystem::Produce(GameObjectRegistry& objects, IslandEconomyManager& economy, TreeRegistry& trees, GameObjectId id, uint64_t tick) {
    ProductionComponent& production = objects.Production(id);
    const ProductionChain& chain = ChainOf(objects, id);
    const IslandStorage* storage = economy.Find(objects.Building(id).island);
    production.productivity = 0;

    if (!objects.Logistics(id).connected || !storage) {
        production.status = ProducerStatus::NoRoad;
        return;
    }
    int workforce = storage->workforce[chain.workforceTier];
    if (workforce == 0) {
        production.status = ProducerStatus::NoWorkforce;
        return;
    }
    if (production.locationFactor == 0) {
        production.status = ProducerStatus::BadLocation;
        return;
    }
    if (production.output >= PRODUCER_BUFFER) {
        production.status = ProducerStatus::OutputFull;
        return;
    }
    for (int i = 0; i < chain.inputCount; i++) {
        if (production.inputs[i] == 0) {
            production.status = ProducerStatus::MissingInput;
            return;
        }
    }

    production.status = ProducerStatus::Working;
    production.productivity = (int16_t)(workforce * production.locationFactor / 1000);
    production.progress += production.productivity;
    const int32_t cycle = (int32_t)chain.cycleTicks * 1000;
    if (production.progress < cycle) return;

    // A cycle is done: inputs in, one output out
    production.progress -= cycle;
    for (int i = 0; i < chain.inputCount; i++) production.inputs[i]--;
    production.output++;
    production.cycles++;

    if (chain.rule == LocationRule::Trees) {
        // The lumberjack cuts down the nearest tree in its reach; the next cycles count one fewer
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::ivec2 minTile, tiles;
        FootprintOf(anchor, minTile, tiles);
        glm::ivec2 center = glm::ivec2(anchor.origin.x, anchor.origin.z) + anchor.footprint / 2;
        int radius = chain.radius;
        trees.FellNearest(center, (minTile - radius) * TILE_SIZE, (minTile + tiles + radius) * TILE_SIZE - 1, tick);
    }
}
