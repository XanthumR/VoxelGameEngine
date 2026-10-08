#include "Economy/Production.h"

#include "Economy/IslandEconomy.h"
#include "Economy/ProductionChains.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Logistics.h"
#include "Simulation/ProducerLocation.h"
#include "Simulation/TreeRegistry.h"

#include <algorithm>

static_assert(WAREHOUSE_ROAD_RANGE < CART_PATH_MAX, "a cart route must fit the warehouse road range");

namespace {

const glm::ivec2 NEIGHBOURS[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

uint16_t WarehouseDistance(const RoadNetwork& roads, glm::ivec2 tile) {
    const RoadTile* road = roads.Find(tile);
    return road ? road->distance : RoadTile::UNREACHED;
}

// The cart is back: whatever it carries goes into the producer's buffers
void CartArrivesHome(ProductionComponent& production) {
    for (size_t i = 0; i < production.inputs.size(); i++) {
        production.inputs[i] = (uint8_t)std::min(255, production.inputs[i] + production.cartInputs[i]);
        production.cartInputs[i] = 0;
    }
    production.output = (uint8_t)std::min(255, production.output + production.cartOutput); // Only after a cut road
    production.cartOutput = 0;
    production.cartState = CartState::Idle;
    production.cartWaitTicks = 0;
    production.cartPosition = 0;
}

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
    m_CartsOnRoad = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || !IsProducer(objects, id)) continue;
        Produce(objects, economy, trees, id, tick);
        UpdateCart(objects, economy, roads, id);
        m_Producers++;
        if (objects.Production(id).status == ProducerStatus::Working) m_Working++;
        if (objects.Production(id).cartState != CartState::Idle) m_CartsOnRoad++;
    }
}

void ProductionSystem::RecallCart(ProductionComponent& production) {
    CartArrivesHome(production);
}

bool ProductionSystem::FindCartPath(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, ProductionComponent& production) {
    glm::ivec2 start(0);
    uint16_t best = RoadTile::UNREACHED;
    ForEachTileAround(minTile, tiles, [&](glm::ivec2 tile) {
        uint16_t distance = WarehouseDistance(roads, tile);
        if (distance < best) {
            best = distance;
            start = tile;
        }
    });
    if (best == RoadTile::UNREACHED || best > CART_PATH_MAX) return false;

    // Downhill: every reached tile but the warehouse's own has a neighbour one closer
    glm::ivec2 tile = start;
    int length = 0;
    production.cartPath[length++] = tile;
    for (int distance = best; distance > 1; distance--) {
        bool stepped = false;
        for (const glm::ivec2& step : NEIGHBOURS) {
            if (WarehouseDistance(roads, tile + step) == distance - 1) {
                tile += step;
                stepped = true;
                break;
            }
        }
        if (!stepped) return false;
        production.cartPath[length++] = tile;
    }
    production.cartPathLength = (uint8_t)length;
    return true;
}

void ProductionSystem::UpdateCart(GameObjectRegistry& objects, IslandEconomyManager& economy, const RoadNetwork& roads, GameObjectId id) {
    ProductionComponent& production = objects.Production(id);
    const ProductionChain& chain = ChainOf(objects, id);
    IslandId island = objects.Building(id).island;
    const IslandStorage* storage = economy.Find(island);
    const int last = std::max(0, production.cartPathLength - 1);

    switch (production.cartState) {
    case CartState::Idle: {
        if (!objects.Logistics(id).connected || !storage) {
            production.cartWaitTicks = 0;
            return;
        }
        // Something to fetch: an input below the buffer that the island has; urgent when it ran out
        bool fetch = false, starved = false;
        for (int i = 0; i < chain.inputCount; i++) {
            if (storage->Amount(chain.inputs[i]) == 0) continue;
            fetch |= production.inputs[i] < PRODUCER_BUFFER;
            starved |= production.inputs[i] == 0;
        }
        if (production.output == 0 && !fetch) {
            production.cartWaitTicks = 0;
            return;
        }
        production.cartWaitTicks++;
        bool full = production.output >= CART_CAPACITY;
        if (!full && !starved && production.cartWaitTicks < CART_MAX_WAIT_TICKS) return;

        glm::ivec2 minTile, tiles;
        FootprintOf(objects.Anchor(id), minTile, tiles);
        if (!FindCartPath(roads, minTile, tiles, production)) return;
        production.cartOutput = (uint8_t)std::min<int>(production.output, CART_CAPACITY);
        production.output = (uint8_t)(production.output - production.cartOutput);
        production.cartState = CartState::ToWarehouse;
        production.cartPosition = 0;
        production.cartWaitTicks = 0;
        return;
    }

    case CartState::ToWarehouse: {
        int tile = production.cartPosition / CART_TILE;
        int ahead = std::min(tile + 1, last);
        if (!roads.IsRoad(production.cartPath[tile]) || !roads.IsRoad(production.cartPath[ahead])) {
            production.cartState = CartState::ToProducer; // The road was cut: home with the cargo
            return;
        }
        production.cartPosition = std::min(production.cartPosition + CART_SPEED, last * CART_TILE);
        if (production.cartPosition == last * CART_TILE) {
            production.cartState = CartState::Unloading;
            production.cartWaitTicks = 0;
        }
        return;
    }

    case CartState::Unloading: {
        if (!roads.IsRoad(production.cartPath[last]) || !storage) {
            production.cartState = CartState::ToProducer;
            return;
        }
        if (production.cartWaitTicks < CART_UNLOAD_TICKS) {
            production.cartWaitTicks++;
            return;
        }
        if (production.cartOutput > 0) {
            production.cartOutput = (uint8_t)(production.cartOutput - economy.Add(island, chain.output, production.cartOutput));
            if (production.cartOutput > 0) return; // Storage is full: wait for room
        }
        for (int i = 0; i < chain.inputCount; i++) {
            int wanted = std::min(CART_CAPACITY, PRODUCER_BUFFER - production.inputs[i] - production.cartInputs[i]);
            if (wanted > 0) production.cartInputs[i] = (uint8_t)(production.cartInputs[i] + economy.Remove(island, chain.inputs[i], wanted));
        }
        production.cartState = CartState::ToProducer;
        production.cartWaitTicks = 0;
        return;
    }

    case CartState::ToProducer: {
        // Only the tile it drives to matters: after turning around at a gap, the tile it was heading
        // for (behind it now) is the missing one
        int ahead = production.cartPosition == 0 ? 0 : (production.cartPosition - 1) / CART_TILE;
        if (!roads.IsRoad(production.cartPath[ahead])) {
            CartArrivesHome(production); // No way back by road: it finds its way home
            return;
        }
        production.cartPosition = std::max(0, production.cartPosition - CART_SPEED);
        if (production.cartPosition == 0) CartArrivesHome(production);
        return;
    }
    }
}

void ProductionSystem::UpdateLocations(GameObjectRegistry& objects, IslandRegistry& islands, const OccupancyGrid& occupancy, const RoadNetwork& roads,
    TreeRegistry& trees) {
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || !IsProducer(objects, id)) continue;
        glm::ivec2 minTile, tiles;
        FootprintOf(objects.Anchor(id), minTile, tiles);
        const ProductionChain& chain = ChainOf(objects, id);
        if (chain.rule == LocationRule::Modules) {
            objects.Production(id).locationFactor = (int16_t)std::min(1000, CountModules(objects, id) * 1000 / chain.fullSpeedCount);
            continue;
        }
        LocationReport report = EvaluateLocation(chain, minTile, tiles, islands, occupancy, roads, trees);
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
