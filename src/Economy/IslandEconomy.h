#pragma once

#include "Economy/PopulationNeeds.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/ItemType.h"

#include <array>
#include <cstdint>
#include <vector>

constexpr int WAREHOUSE_CAPACITY = 50; // Storage per good added by each warehouse on the island

// The goods of one settled island. All warehouses on an island share one store (as in Anno).
struct IslandStorage {
    IslandId island = NO_ISLAND;
    int warehouseCount = 0;
    bool seeded = false; // Got its starting goods (only the first warehouse ever does)
    std::array<int, ITEM_COUNT> amounts = {};

    // Population, filled in by PopulationSystem once per second
    std::array<int, TIER_COUNT> population = {};         // Residents per tier
    std::array<int, TIER_COUNT> suppliedResidents = {};  // Of those, in houses that a marketplace serves
    std::array<std::array<int16_t, MAX_NEEDS>, TIER_COUNT> supply = {}; // Per tier and need: smoothed per mille
    // Goods owed per tier and need, in 1/60000 of a good (consumption is per minute, cycles per second)
    std::array<std::array<int32_t, MAX_NEEDS>, TIER_COUNT> owed = {};

    int CapacityPerItem() const { return warehouseCount * WAREHOUSE_CAPACITY; }
    int Amount(ItemType item) const { return amounts[(size_t)item]; }
};

// The per-island economies. An island gets its storage when its first warehouse is built.
class IslandEconomyManager {
public:
    static constexpr size_t MAX_ISLANDS = 256;

    // Starting goods of a new settlement
    static constexpr std::array<int, ITEM_COUNT> STARTING_GOODS = { 20, 30, 0, 0, 0, 0, 0 };

    IslandEconomyManager();

    void OnWarehouseAdded(IslandId island);
    // Capacity shrinks; goods above the new capacity are lost
    void OnWarehouseRemoved(IslandId island);

    // nullptr for an island without a warehouse ever
    IslandStorage* Find(IslandId island);
    const IslandStorage* Find(IslandId island) const;

    // Return how much actually moved: Add stops at the capacity, Remove at what is stored
    int Add(IslandId island, ItemType item, int amount);
    int Remove(IslandId island, ItemType item, int amount);
    void AddAll(IslandId island, int amount); // Every good (debug button until production exists)

    // Settled islands in a stable order (for deterministic iteration)
    size_t IslandSlotCount() const { return m_Islands.size(); }
    IslandStorage& IslandAt(size_t index) { return m_Islands[index]; }
    const IslandStorage& IslandAt(size_t index) const { return m_Islands[index]; }

    size_t SettledIslandCount() const { return m_Islands.size(); }

private:
    std::vector<IslandStorage> m_Islands; // Few islands: a linear search is fine
};
