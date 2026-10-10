#include "Economy/IslandEconomy.h"
#include "Economy/PopulationSystem.h"
#include "Economy/Treasury.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Logistics.h"
#include "Simulation/RoadNetwork.h"

#include <gtest/gtest.h>

namespace {

constexpr IslandId ISLAND = 1;
constexpr int FISH_NEED = 1;         // Need index of Fish in both tiers
constexpr int TICKS_PER_MINUTE = 600;

// A settled island without voxels: a warehouse at (0,0), a road along z = 1, a marketplace at
// (6,2) and houses next to the road. Runs logistics and population like Simulation::FixedUpdate.
struct PopulationFixture {
    GameObjectRegistry objects;
    RoadNetwork roads;
    LogisticsSystem logistics;
    IslandEconomyManager economy;
    PopulationSystem population;
    uint32_t buildingsRevision = 0;
    uint64_t tick = 0;

    explicit PopulationFixture(bool withMarket = true) {
        AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
        economy.OnWarehouseAdded(ISLAND);
        economy.Find(ISLAND)->warehouseCount = 10; // Room for 500 of each good
        for (int x = 4; x <= 40; x++) roads.Add({ x, 1 });
        if (withMarket) AddBuilding(BUILDING_MARKETPLACE, { 6, 2 });
    }

    GameObjectId AddBuilding(uint16_t type, glm::ivec2 minTile) {
        GameObjectId id = objects.Create();
        objects.Building(id).type = type;
        objects.Building(id).island = ISLAND;
        const BuildingType& building = BUILDING_TYPES[type];
        objects.Anchor(id).origin = glm::ivec3(minTile.x * TILE_SIZE, 0, minTile.y * TILE_SIZE);
        objects.Anchor(id).footprint = glm::ivec2(building.footprintWidth, building.footprintDepth) * TILE_SIZE;
        buildingsRevision++;
        return id;
    }

    GameObjectId AddHouse(int x) { return AddBuilding(BUILDING_FARMER_HOUSE, { x, 2 }); }

    void Run(int ticks) {
        for (int i = 0; i < ticks; i++) {
            logistics.Update(objects, roads, buildingsRevision);
            population.Update(objects, economy, tick++);
        }
    }

    IslandStorage& Storage() { return *economy.Find(ISLAND); }
    void Stock(ItemType item, int amount) { Storage().amounts[(size_t)item] = amount; }
    int Amount(ItemType item) { return Storage().Amount(item); }
};

} // namespace

TEST(PopulationTest, SuppliedHouseFillsUp) {
    PopulationFixture f;
    f.Stock(ItemType::Planks, 0); // No upgrade in this test
    f.Stock(ItemType::Fish, 200);
    f.Stock(ItemType::WorkClothes, 200);
    GameObjectId house = f.AddHouse(12);
    f.Run(60 * 10);
    EXPECT_EQ(f.objects.Residence(house).residents, 10);
    EXPECT_EQ(f.Storage().population[TIER_FARMERS], 10);
}

TEST(PopulationTest, ResidentsMoveInOneEveryTwoSeconds) {
    PopulationFixture f;
    f.Stock(ItemType::Planks, 0);
    f.Stock(ItemType::Fish, 200);
    f.Stock(ItemType::WorkClothes, 200);
    GameObjectId house = f.AddHouse(12);
    f.Run(PopulationSystem::GROWTH_INTERVAL * 3);
    EXPECT_LE(f.objects.Residence(house).residents, 3);
    EXPECT_GE(f.objects.Residence(house).residents, 1);
}

TEST(PopulationTest, NoMarketplaceNoResidents) {
    PopulationFixture f(false);
    f.Stock(ItemType::Fish, 200);
    f.Stock(ItemType::WorkClothes, 200);
    GameObjectId house = f.AddHouse(12);
    f.Run(60 * 10);
    EXPECT_EQ(f.objects.Residence(house).residents, 0);
}

TEST(PopulationTest, WithoutFishOnlyMarketAndClothesResidents) {
    PopulationFixture f;
    f.Stock(ItemType::Planks, 0);
    f.Stock(ItemType::Fish, 0);
    f.Stock(ItemType::WorkClothes, 200);
    GameObjectId house = f.AddHouse(12);
    f.Run(60 * 10);
    EXPECT_EQ(f.objects.Residence(house).residents, 2 + 4); // Market + Work Clothes
}

TEST(PopulationTest, ShortageMakesResidentsLeave) {
    PopulationFixture f;
    f.Stock(ItemType::Planks, 0);
    f.Stock(ItemType::Fish, 200);
    f.Stock(ItemType::WorkClothes, 200);
    GameObjectId house = f.AddHouse(12);
    f.Run(60 * 10);
    ASSERT_EQ(f.objects.Residence(house).residents, 10);
    f.Stock(ItemType::Fish, 0);
    f.Stock(ItemType::WorkClothes, 0);
    f.Run(60 * 10);
    EXPECT_EQ(f.objects.Residence(house).residents, 2); // Only the marketplace is left
}

TEST(PopulationTest, ConsumesAtTheConfiguredRate) {
    PopulationFixture f;
    f.Stock(ItemType::Planks, 0);
    f.Stock(ItemType::Fish, 400);
    f.Stock(ItemType::WorkClothes, 400);
    // Ten full farmer houses on both sides of the road: 100 residents eat 0.05 fish each per minute
    for (int i = 0; i < 5; i++) {
        for (int z : { 2, -2 }) f.objects.Residence(f.AddBuilding(BUILDING_FARMER_HOUSE, { 12 + i * 3, z })).residents = 10;
    }
    f.Storage().supply[TIER_FARMERS] = { 1000, 1000, 1000, 0 }; // Already well supplied: nobody leaves
    f.Run(1); // Logistics
    int before = f.Amount(ItemType::Fish);
    f.Run(10 * TICKS_PER_MINUTE);
    int eaten = before - f.Amount(ItemType::Fish);
    EXPECT_NEAR(eaten, 100 * 50 * 10 / 1000, 1); // 50 fish in 10 minutes
    EXPECT_EQ(f.Storage().population[TIER_FARMERS], 100);
}

TEST(PopulationTest, LowerTierGetsScarceGoodsFirst) {
    PopulationFixture f;
    f.Stock(ItemType::Planks, 0);
    GameObjectId farmer = f.AddHouse(12);
    GameObjectId worker = f.AddBuilding(BUILDING_WORKER_HOUSE, { 16, 2 });
    f.objects.Residence(farmer).residents = 10;
    f.objects.Residence(worker).residents = 20;
    f.Run(1); // Logistics, and the first consumption cycle at tick 0

    // Both tiers owe a whole fish, but there is only one
    f.Stock(ItemType::Fish, 1);
    f.Storage().owed[TIER_FARMERS][FISH_NEED] = PopulationSystem::OWED_PER_GOOD;
    f.Storage().owed[TIER_WORKERS][FISH_NEED] = PopulationSystem::OWED_PER_GOOD;
    f.Run(PopulationSystem::CONSUMPTION_INTERVAL); // Through the next cycle
    EXPECT_EQ(f.Amount(ItemType::Fish), 0);
    EXPECT_LT(f.Storage().owed[TIER_FARMERS][FISH_NEED], PopulationSystem::OWED_PER_GOOD); // Paid
    EXPECT_GE(f.Storage().owed[TIER_WORKERS][FISH_NEED], PopulationSystem::OWED_PER_GOOD); // Still owed
}

TEST(PopulationTest, FullySuppliedHouseWaitsForThePlayerToUpgrade) {
    PopulationFixture f;
    f.Stock(ItemType::Fish, 200);
    f.Stock(ItemType::WorkClothes, 200);
    f.Stock(ItemType::Planks, 10);
    GameObjectId house = f.AddHouse(12);
    f.Run(90 * 10);
    // Ready, but nothing happens on its own
    EXPECT_EQ(f.objects.Building(house).type, BUILDING_FARMER_HOUSE);
    EXPECT_TRUE(PopulationSystem::IsReadyToUpgrade(f.objects, house));
    EXPECT_TRUE(PopulationSystem::CanUpgrade(f.objects, f.economy, house));
    EXPECT_EQ(f.Amount(ItemType::Planks), 10);
    EXPECT_EQ(f.population.Upgrades(), 0u);

    f.population.RequestUpgrade(house);
    f.Run(1);
    EXPECT_EQ(f.objects.Building(house).type, BUILDING_WORKER_HOUSE);
    EXPECT_EQ(f.Amount(ItemType::Planks), 10 - UpgradeCost(TIER_FARMERS).planks);
    EXPECT_EQ(f.population.Upgrades(), 1u);
    ASSERT_EQ(f.population.LookChanges().size(), 1u);
    EXPECT_EQ(f.population.LookChanges()[0], house);
    EXPECT_FALSE(PopulationSystem::IsReadyToUpgrade(f.objects, house)); // Starts over in the new tier
}

TEST(PopulationTest, NoUpgradeWithoutPlanks) {
    PopulationFixture f;
    f.Stock(ItemType::Fish, 200);
    f.Stock(ItemType::WorkClothes, 200);
    f.Stock(ItemType::Planks, 1);
    GameObjectId house = f.AddHouse(12);
    f.Run(90 * 10);
    EXPECT_TRUE(PopulationSystem::IsReadyToUpgrade(f.objects, house));
    EXPECT_FALSE(PopulationSystem::CanUpgrade(f.objects, f.economy, house));
    f.population.RequestUpgrade(house);
    f.Run(1);
    EXPECT_EQ(f.objects.Building(house).type, BUILDING_FARMER_HOUSE);
    EXPECT_EQ(f.Amount(ItemType::Planks), 1);

    // A request does not wait: with the planks there, the player asks again
    f.Stock(ItemType::Planks, UpgradeCost(TIER_FARMERS).planks);
    f.Run(1);
    EXPECT_EQ(f.objects.Building(house).type, BUILDING_FARMER_HOUSE);
    f.population.RequestUpgrade(house);
    f.Run(1);
    EXPECT_EQ(f.objects.Building(house).type, BUILDING_WORKER_HOUSE);
}

TEST(PopulationTest, HouseNotReadyCannotUpgrade) {
    PopulationFixture f;
    f.Stock(ItemType::Fish, 0); // A need unmet: never ready
    f.Stock(ItemType::WorkClothes, 200);
    f.Stock(ItemType::Planks, 10);
    GameObjectId house = f.AddHouse(12);
    f.Run(90 * 10);
    EXPECT_FALSE(PopulationSystem::IsReadyToUpgrade(f.objects, house));
    f.population.RequestUpgrade(house);
    f.Run(1);
    EXPECT_EQ(f.objects.Building(house).type, BUILDING_FARMER_HOUSE);
    EXPECT_EQ(f.Amount(ItemType::Planks), 10);
}

TEST(PopulationTest, WorkerHouseWithoutSupplyDowngrades) {
    PopulationFixture f;
    f.Stock(ItemType::Planks, 0);
    GameObjectId house = f.AddBuilding(BUILDING_WORKER_HOUSE, { 12, 2 });
    f.objects.Residence(house).residents = 8; // Fewer than a farmer house holds
    f.Run(PopulationSystem::DOWNGRADE_TICKS - 1);
    EXPECT_EQ(f.objects.Building(house).type, BUILDING_WORKER_HOUSE);
    f.Run(2);
    EXPECT_EQ(f.objects.Building(house).type, BUILDING_FARMER_HOUSE);
    EXPECT_LE(f.objects.Residence(house).residents, POPULATION_TIERS[TIER_FARMERS].maxResidents);
    EXPECT_EQ(f.population.Downgrades(), 1u);
    EXPECT_EQ(f.population.LookChanges().size(), 1u);
}

TEST(PopulationTest, TargetResidentsFromSupply) {
    const PopulationTier& farmers = POPULATION_TIERS[TIER_FARMERS];
    EXPECT_EQ(TargetResidents(farmers, { 1000, 1000, 1000, 0 }), 10);
    EXPECT_EQ(TargetResidents(farmers, { 1000, 500, 0, 0 }), 4);
    EXPECT_EQ(TargetResidents(farmers, { 0, 0, 0, 0 }), 0);
}

// A worker house: every good and the marketplace, but no school in reach until one is built
TEST(PopulationTest, ServiceNeedsComeFromTheirBuildingsReach) {
    PopulationFixture f;
    for (ItemType item : { ItemType::Fish, ItemType::WorkClothes, ItemType::Sausages, ItemType::Bread, ItemType::Soap }) f.Stock(item, 200);
    GameObjectId house = f.AddBuilding(BUILDING_WORKER_HOUSE, { 12, 2 });
    constexpr int SCHOOL_NEED = 6; // Need index of the School for Workers
    ASSERT_EQ(POPULATION_TIERS[TIER_WORKERS].needs[SCHOOL_NEED].service, ServiceType::School);
    f.Run(30 * 10);
    EXPECT_EQ(f.objects.Residence(house).needSupply[SCHOOL_NEED], 0);
    EXPECT_GT(f.objects.Residence(house).needSupply[0], 0); // The marketplace

    f.AddBuilding(BUILDING_SCHOOL, { 16, 2 });
    f.Run(30 * 10);
    EXPECT_EQ(f.objects.Residence(house).needSupply[SCHOOL_NEED], 1000);
}

TEST(PopulationTest, ArtisanUpgradeTakesBricks) {
    const BuildingCost& cost = UpgradeCost(TIER_WORKERS);
    EXPECT_GT(cost.bricks, 0);
    PopulationFixture f;
    GameObjectId house = f.AddBuilding(BUILDING_WORKER_HOUSE, { 12, 2 });
    f.objects.Residence(house).upgradeTicks = PopulationSystem::UPGRADE_TICKS;
    f.Stock(ItemType::Planks, cost.planks);
    EXPECT_FALSE(PopulationSystem::CanUpgrade(f.objects, f.economy, house)); // No bricks
    f.Stock(ItemType::Bricks, cost.bricks);
    EXPECT_TRUE(PopulationSystem::CanUpgrade(f.objects, f.economy, house));
}
