#pragma once

#include "Economy/PopulationNeeds.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Placement.h"

#include <array>
#include <cstdint>

class IslandEconomyManager;

// Construction materials, taken from the island's storage (in BuildingCost's order)
constexpr int MATERIAL_COUNT = 3;
constexpr std::array<ItemType, MATERIAL_COUNT> MATERIALS = { ItemType::Planks, ItemType::Bricks, ItemType::SteelBeams };

// What a building costs to place and to keep (coins per minute). Index = building type. A house
// type that only comes from upgrades costs its materials when a house upgrades to it.
struct BuildingCost {
    int16_t coins;
    int16_t planks;
    int16_t upkeep; // Coins per minute
    int16_t bricks = 0;
    int16_t steelBeams = 0;

    std::array<int, MATERIAL_COUNT> Materials() const { return { planks, bricks, steelBeams }; }
};

constexpr std::array<BuildingCost, BUILDING_TYPES.size()> BUILDING_COSTS = { {
    { 300, 0, 5 },          // Warehouse (it creates the storage the planks would come from)
    { 50, 2, 0 },           // Farmer House
    { 200, 4, 15 },         // Marketplace
    { 0, 2, 0 },            // Worker House (upgrade)
    { 150, 3, 10 },         // Fishery
    { 100, 0, 5 },          // Lumberjack
    { 150, 4, 10 },         // Sawmill
    { 150, 3, 10 },         // Sheep Farm
    { 250, 6, 25 },         // Framework Knitter
    { 250, 6, 20 },         // Pig Farm
    { 300, 8, 30 },         // Slaughterhouse
    { 400, 6, 10 },         // Harbor
    { 30, 0, 0 },           // Sheepfold
    { 40, 0, 0 },           // Pigsty
    { 400, 6, 20 },         // School
    { 150, 3, 10 },         // Grain Farm
    { 20, 0, 0 },           // Wheat Field
    { 250, 6, 15 },         // Flour Mill
    { 300, 6, 20 },         // Bakery
    { 300, 6, 20 },         // Rendering Works
    { 400, 4, 25, 4 },      // Soap Factory
    { 200, 4, 15 },         // Clay Pit
    { 300, 6, 20 },         // Brick Factory
    { 0, 2, 0, 3 },         // Artisan House (upgrade)
    { 1000, 6, 50, 10, 2 }, // Variety Theatre
    { 200, 4, 10 },         // Cattle Farm
    { 30, 0, 0 },           // Pasture
    { 400, 4, 25, 4 },      // Iron Mine
    { 200, 4, 10 },         // Charcoal Kiln
    { 500, 4, 30, 8 },      // Furnace
    { 600, 4, 40, 8 },      // Steelworks
    { 700, 4, 40, 6, 2 },   // Cannery
    { 800, 4, 50, 6, 3 },   // Sewing Machine Factory
} };

// Coins per minute ten residents pay with every need met, per tier
constexpr std::array<int, TIER_COUNT> TAX_PER_TEN_RESIDENTS = { 10, 25, 45 };

// What a house of this tier takes to move up a tier: the materials of the next tier's house
constexpr const BuildingCost& UpgradeCost(int tier) {
    return BUILDING_COSTS[RESIDENCE_FOR_TIER[tier + 1]];
}

constexpr int64_t STARTING_COINS = 3000;

// The player's coins (shared by all islands). Every tick houses pay taxes, scaled by how well
// their needs are met, and buildings cost their upkeep; the balance may go negative, which only
// stops building. Integer math in thousandths of a coin per minute, so a run is reproducible.
class Treasury {
public:
    void Update(const GameObjectRegistry& objects);

    // NotEnoughCoins, NotEnoughPlanks (or another material) or None
    PlacementError Check(uint16_t type, IslandId island, const IslandEconomyManager& economy) const;
    // The first material the island lacks for this cost (NotEnoughPlanks, ...), None when it has them all
    static PlacementError CheckMaterials(const BuildingCost& cost, IslandId island, const IslandEconomyManager& economy);
    static void TakeMaterials(const BuildingCost& cost, IslandId island, IslandEconomyManager& economy);
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
