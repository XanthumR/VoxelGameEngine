#pragma once

#include "Economy/PopulationNeeds.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Placement.h"

#include <array>
#include <cstdint>

class IslandEconomyManager;

// What a building costs to place and to keep (coins per minute). Index = building type.
struct BuildingCost {
    int16_t coins;
    int16_t planks; // From the island's storage
    int16_t upkeep; // Coins per minute
};

constexpr std::array<BuildingCost, BUILDING_TYPES.size()> BUILDING_COSTS = { {
    { 300, 0, 5 },   // Warehouse (it creates the storage the planks would come from)
    { 50, 2, 0 },    // Farmer House
    { 200, 4, 15 },  // Marketplace
    { 0, 0, 0 },     // Worker House (only from upgrades)
    { 150, 3, 10 },  // Fishery
    { 100, 0, 5 },   // Lumberjack
    { 150, 4, 10 },  // Sawmill
    { 150, 3, 10 },  // Sheep Farm
    { 250, 6, 25 },  // Framework Knitter
    { 250, 6, 20 },  // Pig Farm
    { 300, 8, 30 },  // Slaughterhouse
} };

// Coins per minute ten residents pay with every need met, per tier
constexpr std::array<int, TIER_COUNT> TAX_PER_TEN_RESIDENTS = { 10, 25 };

constexpr int64_t STARTING_COINS = 3000;

// The player's coins (shared by all islands). Every tick houses pay taxes, scaled by how well
// their needs are met, and buildings cost their upkeep; the balance may go negative, which only
// stops building. Integer math in thousandths of a coin per minute, so a run is reproducible.
class Treasury {
public:
    void Update(const GameObjectRegistry& objects);

    // NotEnoughCoins, NotEnoughPlanks or None
    PlacementError Check(uint16_t type, IslandId island, const IslandEconomyManager& economy) const;
    void Pay(uint16_t type, IslandId island, IslandEconomyManager& economy);
    void Refund(uint16_t type, IslandId island, IslandEconomyManager& economy); // Half of the cost

    // A house's taxes in thousandths of a coin per minute
    static int64_t HouseTaxMilli(const GameObjectRegistry& objects, GameObjectId id);

    int64_t Coins() const { return m_Coins; }
    void SetCoins(int64_t coins) { m_Coins = coins; }
    int IncomePerMinute() const { return (int)(m_IncomeMilli / 1000); }
    int UpkeepPerMinute() const { return (int)(m_UpkeepMilli / 1000); }

private:
    static constexpr int64_t TICKS_PER_MINUTE = 600;

    int64_t m_Coins = STARTING_COINS;
    int64_t m_Fraction = 0;    // Thousandths of a coin times TICKS_PER_MINUTE, not yet whole coins
    int64_t m_IncomeMilli = 0; // Per minute, as of the last tick
    int64_t m_UpkeepMilli = 0;
};
