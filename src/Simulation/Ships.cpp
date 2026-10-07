#include "Simulation/Ships.h"

#include "Economy/IslandEconomy.h"
#include "Economy/Treasury.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/OccupancyGrid.h"
#include "World/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace {

// 8 neighbours: the four sides first, then the diagonals (cost 14 against 10)
const glm::ivec2 STEPS[8] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
constexpr uint8_t NOT_REACHED = 255, START = 254;

uint32_t Octile(glm::ivec2 a, glm::ivec2 b) {
    glm::ivec2 d = glm::abs(a - b);
    return (uint32_t)(10 * std::max(d.x, d.y) + 4 * std::min(d.x, d.y));
}

glm::vec2 Middle(glm::ivec2 tile) {
    return glm::vec2(tile) + 0.5f;
}

} // namespace

ShipSystem::ShipSystem(TerrainGenerator& terrain) : m_Terrain(terrain) {
    m_Paths.resize((size_t)MAX_SHIPS * MAX_PATH);
    m_Cost.resize((size_t)SEARCH_SIZE * SEARCH_SIZE);
    m_From.resize((size_t)SEARCH_SIZE * SEARCH_SIZE);
    m_Open.reserve((size_t)SEARCH_SIZE * SEARCH_SIZE);
    m_Scratch.reserve((size_t)SEARCH_SIZE * 4);
    m_Found.reserve((size_t)SEARCH_SIZE * 4);
    m_Generations.fill(1);
}

bool ShipSystem::IsAlive(ShipId id) const {
    uint32_t slot = id & 0xFFFF;
    return id != INVALID_SHIP && slot < (uint32_t)MAX_SHIPS && m_Alive[slot] && m_Generations[slot] == (id >> 16);
}

ShipId ShipSystem::IdAtSlot(int slot) const {
    return m_Alive[slot] ? ((ShipId)m_Generations[slot] << 16) | (ShipId)slot : INVALID_SHIP;
}

int ShipSystem::Count() const {
    return (int)std::count(m_Alive.begin(), m_Alive.end(), true);
}

ShipId ShipSystem::Nearest(glm::vec2 position, float maxTiles) const {
    ShipId best = INVALID_SHIP;
    float bestDistance = maxTiles;
    for (int slot = 0; slot < MAX_SHIPS; slot++) {
        if (!m_Alive[slot]) continue;
        float distance = glm::length(m_Ships[slot].position - position);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = IdAtSlot(slot);
        }
    }
    return best;
}

int ShipSystem::CargoAmount(ShipId id, ItemType item) const {
    if (!IsAlive(id)) return 0;
    int amount = 0;
    for (const CargoSlot& slot : Get(id).cargo) amount += slot.item == item ? slot.amount : 0;
    return amount;
}

ShipId ShipSystem::SettlerNear(glm::ivec2 minTile, glm::ivec2 tiles, int planks) const {
    for (int slot = 0; slot < MAX_SHIPS; slot++) {
        ShipId id = IdAtSlot(slot);
        if (id == INVALID_SHIP || m_Ships[slot].state == ShipState::Sailing || CargoAmount(id, ItemType::Planks) < planks) continue;
        // Distance from the ship to the footprint's rectangle
        glm::vec2 p = m_Ships[slot].position;
        glm::vec2 outside = glm::max(glm::max(glm::vec2(minTile) - p, p - glm::vec2(minTile + tiles)), glm::vec2(0.0f));
        if (glm::length(outside) <= SETTLE_TILES) return id;
    }
    return INVALID_SHIP;
}

PlacementError ShipSystem::CheckBuildCost(uint16_t type, IslandId island, glm::ivec2 minTile, glm::ivec2 tiles,
    const IslandEconomyManager& economy, const Treasury& treasury) const {
    // The first island settled, or one already settled: the usual cost
    if (economy.SettledIslandCount() == 0 || economy.Find(island)) return treasury.Check(type, island, economy);
    const BuildingCost& cost = BUILDING_COSTS[type];
    if (BUILDING_TYPES[type].role != BuildingRole::Storage) return PlacementError::NeedsStorage;
    if (treasury.Coins() < cost.coins) return PlacementError::NotEnoughCoins;
    return SettlerNear(minTile, tiles, cost.planks) == INVALID_SHIP ? PlacementError::NeedsShip : PlacementError::None;
}

void ShipSystem::PayBuildCost(uint16_t type, IslandId island, glm::ivec2 minTile, glm::ivec2 tiles, IslandEconomyManager& economy, Treasury& treasury) {
    if (economy.SettledIslandCount() == 0 || economy.Find(island)) {
        treasury.Pay(type, island, economy);
        return;
    }
    const BuildingCost& cost = BUILDING_COSTS[type];
    treasury.SetCoins(treasury.Coins() - cost.coins);
    ShipId id = SettlerNear(minTile, tiles, cost.planks);
    if (id == INVALID_SHIP) return;
    int left = cost.planks;
    for (CargoSlot& slot : m_Ships[id & 0xFFFF].cargo) {
        if (slot.item != ItemType::Planks) continue;
        int taken = std::min(left, slot.amount);
        slot.amount -= taken;
        left -= taken;
    }
}

bool ShipSystem::IsWater(glm::ivec2 tile, const OccupancyGrid& occupancy) {
    TerrainGenerator::TileKind kind = m_Terrain.TileKindAt(tile.x, tile.y);
    return (kind == TerrainGenerator::TileKind::Sea || kind == TerrainGenerator::TileKind::Cliff) && occupancy.At(tile) == INVALID_GAME_OBJECT;
}

glm::ivec2 ShipSystem::BerthTile(const GameObjectRegistry& objects, GameObjectId harbor) {
    const BuildingComponent& building = objects.Building(harbor);
    const BuildingType& type = BUILDING_TYPES[building.type];
    const VoxelAnchorComponent& anchor = objects.Anchor(harbor);
    glm::ivec2 tiles = FootprintTiles(type, building.rotation);
    glm::ivec2 minTile(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
    // The back row's middle tile, then two steps out (the model's +v is the back), clear of the pier
    glm::ivec2 pierEnd = minTile + RotateToFootprint(type.footprintWidth / 2, type.footprintDepth - 1, building.rotation, tiles);
    glm::ivec2 out = RotateToFootprint(0, 1, building.rotation, tiles) - RotateToFootprint(0, 0, building.rotation, tiles);
    return pierEnd + out * 2;
}

// Samples the straight line between two tile middles every quarter tile
bool ShipSystem::LineIsWater(glm::ivec2 from, glm::ivec2 to, const OccupancyGrid& occupancy) {
    glm::vec2 a = Middle(from), b = Middle(to);
    int samples = (int)std::ceil(glm::length(b - a) * 4.0f);
    for (int i = 1; i < samples; i++) {
        glm::vec2 p = glm::mix(a, b, (float)i / (float)samples);
        if (!IsWater(glm::ivec2(glm::floor(p)), occupancy)) return false;
        // A line through a corner touches both tiles beside it
        glm::vec2 inTile = p - glm::floor(p);
        if ((inTile.x < 0.15f || inTile.x > 0.85f) && (inTile.y < 0.15f || inTile.y > 0.85f)) {
            glm::ivec2 corner = glm::ivec2(glm::floor(p + 0.5f));
            for (glm::ivec2 offset : { glm::ivec2(-1, -1), glm::ivec2(0, -1), glm::ivec2(-1, 0), glm::ivec2(0, 0) }) {
                if (!IsWater(corner + offset, occupancy)) return false;
            }
        }
    }
    return true;
}

bool ShipSystem::FindPath(glm::ivec2 start, glm::ivec2 goal, const OccupancyGrid& occupancy, std::vector<glm::ivec2>& out) {
    out.clear();
    const int size = SEARCH_SIZE;
    glm::ivec2 origin = (start + goal) / 2 - size / 2;
    auto inWindow = [&](glm::ivec2 tile) { glm::ivec2 local = tile - origin; return local.x >= 0 && local.y >= 0 && local.x < size && local.y < size; };
    if (!inWindow(start) || !inWindow(goal)) return false;
    auto indexOf = [&](glm::ivec2 tile) { glm::ivec2 local = tile - origin; return (uint32_t)(local.y * size + local.x); };
    auto tileOf = [&](uint32_t index) { return origin + glm::ivec2((int)(index % size), (int)(index / size)); };
    auto passable = [&](glm::ivec2 tile) { return tile == goal || tile == start || IsWater(tile, occupancy); };

    std::fill(m_Cost.begin(), m_Cost.end(), 0xFFFFFFFFu);
    std::fill(m_From.begin(), m_From.end(), NOT_REACHED);
    m_Open.clear();
    uint32_t startIndex = indexOf(start), goalIndex = indexOf(goal);
    m_Cost[startIndex] = 0;
    m_From[startIndex] = START;
    m_Open.push_back(((uint64_t)Octile(start, goal) << 32) | startIndex);

    bool found = false;
    while (!m_Open.empty()) {
        std::pop_heap(m_Open.begin(), m_Open.end(), std::greater<uint64_t>());
        uint64_t entry = m_Open.back();
        m_Open.pop_back();
        uint32_t index = (uint32_t)entry;
        glm::ivec2 tile = tileOf(index);
        if ((uint32_t)(entry >> 32) > m_Cost[index] + Octile(tile, goal)) continue; // Stale entry
        if (index == goalIndex) {
            found = true;
            break;
        }
        for (uint8_t s = 0; s < 8; s++) {
            glm::ivec2 next = tile + STEPS[s];
            if (!inWindow(next) || !passable(next)) continue;
            if (s >= 4 && (!passable(tile + glm::ivec2(STEPS[s].x, 0)) || !passable(tile + glm::ivec2(0, STEPS[s].y)))) continue; // No corner cutting
            uint32_t nextIndex = indexOf(next);
            uint32_t cost = m_Cost[index] + (s >= 4 ? 14u : 10u);
            if (cost >= m_Cost[nextIndex]) continue;
            m_Cost[nextIndex] = cost;
            m_From[nextIndex] = s;
            m_Open.push_back(((uint64_t)(cost + Octile(next, goal)) << 32) | nextIndex);
            std::push_heap(m_Open.begin(), m_Open.end(), std::greater<uint64_t>());
        }
    }
    if (!found) return false;

    // Walk back from the goal, then keep only the turns the water forces (line of sight)
    m_Scratch.clear();
    for (uint32_t index = goalIndex; m_From[index] != START; index = indexOf(tileOf(index) - STEPS[m_From[index]])) {
        m_Scratch.push_back(tileOf(index));
        if (m_Scratch.size() > (size_t)size * 4) return false;
    }
    m_Scratch.push_back(start);
    std::reverse(m_Scratch.begin(), m_Scratch.end());
    size_t anchor = 0;
    for (size_t i = 1; i < m_Scratch.size(); i++) {
        bool last = i + 1 == m_Scratch.size();
        if (last || !LineIsWater(m_Scratch[anchor], m_Scratch[i + 1], occupancy)) {
            out.push_back(m_Scratch[i]);
            anchor = i;
        }
    }
    return out.size() <= (size_t)MAX_PATH;
}

ShipId ShipSystem::Build(GameObjectId harbor, const GameObjectRegistry& objects, IslandEconomyManager& economy, Treasury& treasury) {
    if (!objects.IsAlive(harbor)) return INVALID_SHIP;
    const BuildingType& type = BUILDING_TYPES[objects.Building(harbor).type];
    if (type.role != BuildingRole::Storage || type.dockRows == 0) return INVALID_SHIP;
    IslandId island = objects.Building(harbor).island;
    const IslandStorage* storage = economy.Find(island);
    if (!storage || storage->Amount(ItemType::Planks) < SHIP_PLANKS || treasury.Coins() < SHIP_COINS) return INVALID_SHIP;
    int slot = -1;
    for (int i = 0; i < MAX_SHIPS && slot < 0; i++) {
        if (!m_Alive[i]) slot = i;
    }
    if (slot < 0) return INVALID_SHIP;

    treasury.SetCoins(treasury.Coins() - SHIP_COINS);
    economy.Remove(island, ItemType::Planks, SHIP_PLANKS);
    m_Alive[slot] = true;
    m_Ships[slot] = Ship();
    m_Ships[slot].position = Middle(BerthTile(objects, harbor));
    // A hair behind, so it is drawn facing out to sea until it first moves
    const BuildingComponent& building = objects.Building(harbor);
    glm::ivec2 tiles = FootprintTiles(type, building.rotation);
    glm::vec2 out = glm::vec2(RotateToFootprint(0, 1, building.rotation, tiles) - RotateToFootprint(0, 0, building.rotation, tiles));
    m_Ships[slot].previous = m_Ships[slot].position - out * 0.001f;
    m_Ships[slot].state = ShipState::Docked;
    m_Ships[slot].harbor = harbor;
    return IdAtSlot(slot);
}

bool ShipSystem::SailTo(ShipId id, glm::ivec2 tile, const GameObjectRegistry& objects, const OccupancyGrid& occupancy, GameObjectId harbor) {
    if (!IsAlive(id)) return false;
    if (harbor != INVALID_GAME_OBJECT) {
        if (!objects.IsAlive(harbor) || BUILDING_TYPES[objects.Building(harbor).type].dockRows == 0) return false;
        tile = BerthTile(objects, harbor);
    } else if (!IsWater(tile, occupancy)) {
        return false; // Only open water, or a harbor's berth
    }
    Ship& ship = m_Ships[id & 0xFFFF];
    glm::ivec2 from = glm::ivec2(glm::floor(ship.position));
    if (!FindPath(from, tile, occupancy, m_Found)) return false;
    const std::vector<glm::ivec2>& found = m_Found;
    int slot = (int)(id & 0xFFFF);
    std::copy(found.begin(), found.end(), m_Paths.begin() + (size_t)slot * MAX_PATH);
    ship.pathLength = (int)found.size();
    ship.pathIndex = 0;
    ship.state = ShipState::Sailing;
    ship.harbor = harbor;
    return true;
}

int ShipSystem::Transfer(ShipId id, ItemType item, int amount, const GameObjectRegistry& objects, IslandEconomyManager& economy) {
    if (!IsAlive(id) || amount == 0) return 0;
    Ship& ship = m_Ships[id & 0xFFFF];
    if (ship.state != ShipState::Docked || !objects.IsAlive(ship.harbor)) return 0;
    IslandId island = objects.Building(ship.harbor).island;
    if (!economy.Find(island)) return 0;

    if (amount > 0) {
        // Into the slot holding this good, else an empty one
        CargoSlot* slot = nullptr;
        for (CargoSlot& s : ship.cargo) {
            if (s.amount > 0 && s.item == item) slot = &s;
        }
        for (CargoSlot& s : ship.cargo) {
            if (!slot && s.amount == 0) slot = &s;
        }
        if (!slot) return 0;
        int moved = economy.Remove(island, item, std::min(amount, SLOT_CAPACITY - (slot->item == item ? slot->amount : 0)));
        if (moved > 0) {
            slot->item = item;
            slot->amount += moved;
        }
        return moved;
    }
    for (CargoSlot& s : ship.cargo) {
        if (s.amount == 0 || s.item != item) continue;
        int moved = economy.Add(island, item, std::min(-amount, s.amount));
        s.amount -= moved;
        return -moved;
    }
    return 0;
}

int ShipSystem::CreateRoute() {
    for (int route = 0; route < MAX_ROUTES; route++) {
        if (m_Routes[route].used) continue;
        m_Routes[route] = TradeRoute();
        m_Routes[route].used = true;
        return route;
    }
    return -1;
}

void ShipSystem::DeleteRoute(int route) {
    if (route < 0 || route >= MAX_ROUTES) return;
    m_Routes[route].used = false;
    for (Ship& ship : m_Ships) {
        if (ship.route == route) ship.route = -1;
    }
}

void ShipSystem::AssignRoute(ShipId id, int route) {
    if (!IsAlive(id)) return;
    Ship& ship = m_Ships[id & 0xFFFF];
    ship.route = (route >= 0 && route < MAX_ROUTES && m_Routes[route].used) ? route : -1;
    ship.stop = 0;
    ship.waitTicks = 0;
}

void ShipSystem::FollowRoute(ShipId id, const GameObjectRegistry& objects, const OccupancyGrid& occupancy, IslandEconomyManager& economy) {
    Ship& ship = m_Ships[id & 0xFFFF];
    const TradeRoute& route = m_Routes[ship.route];
    if (route.stopCount == 0 || ship.state == ShipState::Sailing) return;
    ship.stop %= route.stopCount;

    const RouteStop& stop = route.stops[ship.stop];
    bool atStop = ship.state == ShipState::Docked && ship.harbor == stop.harbor;
    if (!atStop) {
        // On to this stop; a stop whose harbor is gone (or that no sea leads to) is skipped
        if (!SailTo(id, glm::ivec2(0), objects, occupancy, stop.harbor)) ship.stop = (ship.stop + 1) % route.stopCount;
        ship.waitTicks = 0;
        return;
    }

    if (++ship.waitTicks < STOP_TICKS) return;
    // Unload first (while the island's storage is full the ship waits), then load
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (stop.actions[i] != StopAction::Unload) continue;
        for (const CargoSlot& slot : ship.cargo) {
            if (slot.amount > 0 && slot.item == (ItemType)i) Transfer(id, (ItemType)i, -slot.amount, objects, economy);
        }
        for (const CargoSlot& slot : ship.cargo) {
            if (slot.amount > 0 && slot.item == (ItemType)i) return; // Not all of it fit: wait for room
        }
    }
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (stop.actions[i] == StopAction::Load) Transfer(id, (ItemType)i, SLOT_CAPACITY, objects, economy);
    }
    ship.stop = (ship.stop + 1) % route.stopCount;
    ship.waitTicks = 0;
}

void ShipSystem::Update(const GameObjectRegistry& objects, const OccupancyGrid& occupancy, IslandEconomyManager& economy) {
    for (int slot = 0; slot < MAX_SHIPS; slot++) {
        if (!m_Alive[slot]) continue;
        Ship& ship = m_Ships[slot];
        ship.previous = ship.position;
        if (ship.route >= 0) FollowRoute(IdAtSlot(slot), objects, occupancy, economy);
        if (ship.state == ShipState::Docked && !objects.IsAlive(ship.harbor)) ship.state = ShipState::Idle; // Its harbor is gone
        if (ship.state != ShipState::Sailing) continue;

        float remaining = SPEED;
        const glm::ivec2* path = &m_Paths[(size_t)slot * MAX_PATH];
        while (remaining > 0.0f && ship.pathIndex < ship.pathLength) {
            glm::vec2 target = Middle(path[ship.pathIndex]);
            glm::vec2 toTarget = target - ship.position;
            float distance = glm::length(toTarget);
            if (distance <= remaining) {
                ship.position = target;
                remaining -= distance;
                ship.pathIndex++;
            } else {
                ship.position += toTarget / distance * remaining;
                remaining = 0.0f;
            }
        }
        if (ship.pathIndex >= ship.pathLength) {
            bool docked = ship.harbor != INVALID_GAME_OBJECT && objects.IsAlive(ship.harbor);
            ship.state = docked ? ShipState::Docked : ShipState::Idle;
            if (!docked) ship.harbor = INVALID_GAME_OBJECT;
        }
    }
}
