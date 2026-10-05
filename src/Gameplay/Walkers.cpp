#include "Gameplay/Walkers.h"

#include "Economy/IslandEconomy.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Logistics.h"
#include "Simulation/Placement.h"
#include "Simulation/RoadNetwork.h"

#include <algorithm>
#include <cmath>

namespace {

const glm::ivec2 DIRECTIONS[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

} // namespace

WalkerSystem::WalkerSystem() {
    m_Walkers.reserve(MAX_WALKERS);
    m_Figures.reserve(MAX_WALKERS);
    m_SpawnTimers.reserve(IslandEconomyManager::MAX_ISLANDS);
}

// xorshift32: cheap, and the same sequence every run
uint32_t WalkerSystem::Random() {
    m_RandomState ^= m_RandomState << 13;
    m_RandomState ^= m_RandomState >> 17;
    m_RandomState ^= m_RandomState << 5;
    return m_RandomState;
}

size_t WalkerSystem::CountOn(IslandId island) const {
    size_t count = 0;
    for (const Walker& walker : m_Walkers) count += walker.island == island ? 1 : 0;
    return count;
}

// A walker leaves a lived-in house of this island from a road tile next to it
bool WalkerSystem::Spawn(IslandId island, const GameObjectRegistry& objects, const RoadNetwork& roads) {
    uint32_t slots = objects.SlotCount();
    for (uint32_t i = 0; i < slots; i++) {
        uint32_t slot = (m_SpawnCursor + i) % slots;
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingComponent& building = objects.Building(id);
        const BuildingType& type = BUILDING_TYPES[building.type];
        if (building.island != island || type.role != BuildingRole::Residence || objects.Residence(id).residents == 0) continue;

        // The first road tile around the house
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::ivec2 minTile(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
        bool found = false;
        glm::ivec2 start(0);
        ForEachTileAround(minTile, anchor.footprint / TILE_SIZE, [&](glm::ivec2 tile) {
            if (!found && roads.IsRoad(tile)) {
                start = tile;
                found = true;
            }
        });
        if (!found) continue;

        Walker walker;
        walker.island = island;
        walker.tile = start;
        walker.previous = start;
        // Lanes a quarter of the way in from either side of the road
        auto lane = [this]() { return (Random() & 1) ? TILE_SIZE / 4 : TILE_SIZE - 1 - TILE_SIZE / 4; };
        walker.lane = glm::ivec2(lane(), lane());
        walker.progress = 0.0f;
        walker.walked = 0.0f;
        walker.tier = type.tier;
        walker.direction = 0;
        walker.variant = (uint8_t)(Random() & 7);
        walker.next = ChooseNext(walker, roads);
        m_Walkers.push_back(walker); // Within the reserve: callers check MAX_WALKERS
        m_SpawnCursor = slot + 1;    // The next walker comes from another house
        return true;
    }
    return false;
}

// A random road neighbour that is not where it came from; back the way it came at a dead end;
// stays put on a lone tile
glm::ivec2 WalkerSystem::ChooseNext(const Walker& walker, const RoadNetwork& roads) {
    glm::ivec2 options[4];
    int count = 0;
    bool canGoBack = false;
    for (const glm::ivec2& direction : DIRECTIONS) {
        glm::ivec2 tile = walker.tile + direction;
        if (!roads.IsRoad(tile)) continue;
        if (tile == walker.previous && walker.previous != walker.tile) {
            canGoBack = true;
            continue;
        }
        options[count++] = tile;
    }
    if (count > 0) return options[Random() % (uint32_t)count];
    return canGoBack ? walker.previous : walker.tile;
}

void WalkerSystem::Update(float deltaTime, const GameObjectRegistry& objects, const RoadNetwork& roads, const IslandEconomyManager& economy) {
    // Walk; a walker whose road was removed vanishes
    for (size_t i = 0; i < m_Walkers.size();) {
        Walker& walker = m_Walkers[i];
        if (!roads.IsRoad(walker.tile) || !roads.IsRoad(walker.next)) {
            walker = m_Walkers.back();
            m_Walkers.pop_back();
            continue;
        }
        walker.progress += deltaTime * TILES_PER_SECOND;
        if (walker.next != walker.tile) walker.walked += deltaTime * TILES_PER_SECOND * TILE_SIZE;
        while (walker.progress >= 1.0f) {
            walker.progress -= 1.0f;
            walker.previous = walker.tile;
            walker.tile = walker.next;
            walker.next = ChooseNext(walker, roads);
        }
        i++;
    }

    // Match each island's walker count to its population: one leaves a house every SPAWN_INTERVAL,
    // surplus walkers vanish at once
    if (m_SpawnTimers.size() < economy.IslandSlotCount()) m_SpawnTimers.resize(economy.IslandSlotCount(), 0.0f); // Grows with settled islands only
    for (size_t i = 0; i < economy.IslandSlotCount(); i++) {
        const IslandStorage& storage = economy.IslandAt(i);
        int residents = 0;
        for (int tierResidents : storage.population) residents += tierResidents;
        int target = TargetCount(residents);
        int current = (int)CountOn(storage.island);

        for (size_t w = m_Walkers.size(); w-- > 0 && current > target;) {
            if (m_Walkers[w].island != storage.island) continue;
            m_Walkers[w] = m_Walkers.back();
            m_Walkers.pop_back();
            current--;
        }

        m_SpawnTimers[i] = std::max(0.0f, m_SpawnTimers[i] - deltaTime);
        if (current < target && m_SpawnTimers[i] <= 0.0f && m_Walkers.size() < (size_t)MAX_WALKERS) {
            if (Spawn(storage.island, objects, roads)) m_SpawnTimers[i] = SPAWN_INTERVAL;
        }
    }

    // Voxel figures: feet on the road, moving through the lane column of each tile, facing the
    // way they walk, legs and arms following the walk cycle
    m_Figures.clear();
    for (Walker& walker : m_Walkers) {
        glm::ivec2 step = walker.next - walker.tile;
        if (step.x > 0) walker.direction = 0;
        else if (step.x < 0) walker.direction = 1;
        else if (step.y > 0) walker.direction = 2;
        else if (step.y < 0) walker.direction = 3;

        glm::vec2 from = glm::vec2(walker.tile * TILE_SIZE + walker.lane);
        glm::vec2 to = glm::vec2(walker.next * TILE_SIZE + walker.lane);
        glm::ivec2 column = glm::ivec2(glm::floor(glm::mix(from, to, walker.progress) + 0.5f));
        int frame = (int)(walker.walked / STRIDE) & 3;
        int look = WalkerFigure::Pack(walker.tier, walker.direction, frame, walker.variant);
        m_Figures.push_back({ glm::ivec4(column.x, BUILD_GROUND_Y, column.y, look) });
    }
}
