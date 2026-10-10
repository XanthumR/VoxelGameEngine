#include "Economy/IslandEconomy.h"
#include "Economy/Treasury.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"

#include <gtest/gtest.h>

namespace {

constexpr IslandId ISLAND = 1;
constexpr int TICKS_PER_MINUTE = 600;

struct TreasuryFixture {
    GameObjectRegistry objects;
    IslandEconomyManager economy;
    Treasury treasury;

    TreasuryFixture() { treasury.SetCoins(0); }

    GameObjectId Add(uint16_t type) {
        GameObjectId id = objects.Create();
        objects.Building(id).type = type;
        objects.Building(id).island = ISLAND;
        return id;
    }

    // A farmer house with this many residents and every need at this supply (per mille)
    GameObjectId House(int residents, int16_t supply) {
        GameObjectId id = Add(BUILDING_FARMER_HOUSE);
        objects.Residence(id).residents = (uint8_t)residents;
        objects.Residence(id).needSupply.fill(supply);
        return id;
    }

    void RunMinutes(int minutes) {
        for (int i = 0; i < minutes * TICKS_PER_MINUTE; i++) treasury.Update(objects);
    }
};

} // namespace

TEST(TreasuryTest, FullySuppliedHousePaysItsTax) {
    TreasuryFixture f;
    f.House(10, 1000);
    f.RunMinutes(1);
    EXPECT_EQ(f.treasury.Coins(), TAX_PER_TEN_RESIDENTS[TIER_FARMERS]);
    EXPECT_EQ(f.treasury.IncomePerMinute(), TAX_PER_TEN_RESIDENTS[TIER_FARMERS]);
}

TEST(TreasuryTest, TaxFollowsTheNeeds) {
    TreasuryFixture f;
    f.House(10, 500);
    f.RunMinutes(2);
    EXPECT_EQ(f.treasury.Coins(), TAX_PER_TEN_RESIDENTS[TIER_FARMERS]); // Half the tax, over two minutes

    TreasuryFixture empty;
    empty.House(0, 1000);
    empty.House(10, 0);
    empty.RunMinutes(1);
    EXPECT_EQ(empty.treasury.Coins(), 0);
}

TEST(TreasuryTest, UpkeepCanTakeTheBalanceBelowZero) {
    TreasuryFixture f;
    f.Add(BUILDING_SAWMILL);
    f.RunMinutes(3);
    EXPECT_EQ(f.treasury.Coins(), -3 * BUILDING_COSTS[BUILDING_SAWMILL].upkeep);
    EXPECT_EQ(f.treasury.UpkeepPerMinute(), BUILDING_COSTS[BUILDING_SAWMILL].upkeep);
}

TEST(TreasuryTest, CheckNeedsCoinsAndPlanks) {
    TreasuryFixture f;
    f.economy.OnWarehouseAdded(ISLAND);
    f.economy.Find(ISLAND)->amounts[(size_t)ItemType::Planks] = 0;
    const BuildingCost& cost = BUILDING_COSTS[BUILDING_SAWMILL];

    f.treasury.SetCoins(cost.coins - 1);
    EXPECT_EQ(f.treasury.Check(BUILDING_SAWMILL, ISLAND, f.economy), PlacementError::NotEnoughCoins);
    f.treasury.SetCoins(cost.coins);
    EXPECT_EQ(f.treasury.Check(BUILDING_SAWMILL, ISLAND, f.economy), PlacementError::NotEnoughPlanks);
    f.economy.Find(ISLAND)->amounts[(size_t)ItemType::Planks] = cost.planks;
    EXPECT_EQ(f.treasury.Check(BUILDING_SAWMILL, ISLAND, f.economy), PlacementError::None);
    EXPECT_EQ(f.treasury.Check(BUILDING_SAWMILL, ISLAND + 1, f.economy), PlacementError::NotEnoughPlanks); // No storage there
    f.treasury.SetCoins(-5);
    EXPECT_EQ(f.treasury.Check(BUILDING_LUMBERJACK, ISLAND, f.economy), PlacementError::NotEnoughCoins); // In debt
}

TEST(TreasuryTest, DemolishRefundsHalf) {
    TreasuryFixture f;
    f.economy.OnWarehouseAdded(ISLAND);
    f.economy.Find(ISLAND)->amounts[(size_t)ItemType::Planks] = 20;
    f.treasury.SetCoins(1000);
    const BuildingCost& cost = BUILDING_COSTS[BUILDING_SAWMILL];
    f.treasury.Pay(BUILDING_SAWMILL, ISLAND, f.economy);
    EXPECT_EQ(f.treasury.Coins(), 1000 - cost.coins);
    EXPECT_EQ(f.economy.Find(ISLAND)->Amount(ItemType::Planks), 20 - cost.planks);
    f.treasury.Refund(BUILDING_SAWMILL, ISLAND, f.economy);
    EXPECT_EQ(f.treasury.Coins(), 1000 - cost.coins + cost.coins / 2);
    EXPECT_EQ(f.economy.Find(ISLAND)->Amount(ItemType::Planks), 20 - cost.planks + cost.planks / 2);
}

TEST(TreasuryTest, BricksAndSteelBeamsAreMaterialsToo) {
    TreasuryFixture f;
    f.economy.OnWarehouseAdded(ISLAND);
    IslandStorage& storage = *f.economy.Find(ISLAND);
    const BuildingCost& cost = BUILDING_COSTS[BUILDING_THEATRE];
    f.treasury.SetCoins(cost.coins);
    storage.amounts[(size_t)ItemType::Planks] = cost.planks;
    EXPECT_EQ(f.treasury.Check(BUILDING_THEATRE, ISLAND, f.economy), PlacementError::NotEnoughBricks);
    storage.amounts[(size_t)ItemType::Bricks] = cost.bricks;
    EXPECT_EQ(f.treasury.Check(BUILDING_THEATRE, ISLAND, f.economy), PlacementError::NotEnoughSteelBeams);
    storage.amounts[(size_t)ItemType::SteelBeams] = cost.steelBeams;
    EXPECT_EQ(f.treasury.Check(BUILDING_THEATRE, ISLAND, f.economy), PlacementError::None);
    f.treasury.Pay(BUILDING_THEATRE, ISLAND, f.economy);
    EXPECT_EQ(storage.Amount(ItemType::Bricks), 0);
    EXPECT_EQ(storage.Amount(ItemType::SteelBeams), 0);
}
