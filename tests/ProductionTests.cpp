#include "Economy/IslandEconomy.h"
#include "Economy/Production.h"
#include "Economy/ProductionChains.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/Logistics.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/RoadNetwork.h"
#include "Simulation/TreeRegistry.h"
#include "TestWorld.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>

namespace {

constexpr IslandId ISLAND = 1;
const ProductionChain& SawmillChain() { return PRODUCTION_CHAINS[BUILDING_TYPES[BUILDING_SAWMILL].chain]; }

// A settled island without voxels: a warehouse at (0,0), a road along z = 1, producers next to it
struct ProductionFixture {
    GameObjectRegistry objects;
    RoadNetwork roads;
    LogisticsSystem logistics;
    IslandEconomyManager economy;
    IslandRegistry islands{ TestWorld::Get().terrain };
    OccupancyGrid occupancy;
    TreeRegistry trees{ TestWorld::Get().terrain };
    ProductionSystem production;
    uint32_t buildingsRevision = 0;
    uint64_t tick = 0;

    ProductionFixture() {
        AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
        economy.OnWarehouseAdded(ISLAND);
        for (int x = 4; x <= 40; x++) roads.Add({ x, 1 });
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

    GameObjectId AddSawmill(int x, int woodInStock = PRODUCER_BUFFER) {
        GameObjectId id = AddBuilding(BUILDING_SAWMILL, { x, 2 });
        objects.Production(id).inputs[0] = (uint8_t)woodInStock;
        return id;
    }

    void SetFarmers(int residents) { economy.Find(ISLAND)->population[TIER_FARMERS] = residents; }

    void Run(int ticks, bool withLogistics = true) {
        for (int i = 0; i < ticks; i++) {
            if (withLogistics) logistics.Update(objects, roads, buildingsRevision);
            production.Update(objects, economy, islands, occupancy, roads, trees, buildingsRevision * 31 + trees.Revision(), tick++);
        }
    }
};

} // namespace

TEST(ProductionTest, CycleTakesItsTimeAtFullWorkforce) {
    ProductionFixture f;
    f.SetFarmers(10);
    GameObjectId sawmill = f.AddSawmill(12);
    f.Run(SawmillChain().cycleTicks - 1);
    EXPECT_EQ(f.objects.Production(sawmill).output, 0);
    EXPECT_EQ(f.objects.Production(sawmill).status, ProducerStatus::Working);
    f.Run(1);
    EXPECT_EQ(f.objects.Production(sawmill).output, 1);
    EXPECT_EQ(f.objects.Production(sawmill).inputs[0], PRODUCER_BUFFER - 1);
    EXPECT_EQ(f.objects.Production(sawmill).cycles, 1u);
}

TEST(ProductionTest, WorkforceShortageSlowsEveryProducerOfTheTier) {
    ProductionFixture f;
    GameObjectId first = f.AddSawmill(12);
    GameObjectId second = f.AddSawmill(16);
    f.SetFarmers(SawmillChain().workforce); // Half of the two sawmills' jobs
    f.Run(1);
    EXPECT_EQ(f.economy.Find(ISLAND)->jobs[TIER_FARMERS], 2 * SawmillChain().workforce);
    EXPECT_EQ(f.economy.Find(ISLAND)->workforce[TIER_FARMERS], 500);
    EXPECT_EQ(f.objects.Production(first).productivity, 500);

    f.Run(2 * SawmillChain().cycleTicks - 2);
    EXPECT_EQ(f.objects.Production(first).output, 0);
    EXPECT_EQ(f.objects.Production(second).output, 0);
    f.Run(1);
    EXPECT_EQ(f.objects.Production(first).output, 1); // Twice as long
    EXPECT_EQ(f.objects.Production(second).output, 1);
}

TEST(ProductionTest, NoInputNoProgress) {
    ProductionFixture f;
    f.SetFarmers(10);
    GameObjectId sawmill = f.AddSawmill(12, 0);
    f.Run(SawmillChain().cycleTicks * 2);
    EXPECT_EQ(f.objects.Production(sawmill).status, ProducerStatus::MissingInput);
    EXPECT_EQ(f.objects.Production(sawmill).progress, 0);
    EXPECT_EQ(f.objects.Production(sawmill).output, 0);
}

TEST(ProductionTest, StopsWhenTheOutputIsFull) {
    ProductionFixture f;
    f.SetFarmers(10);
    GameObjectId sawmill = f.AddSawmill(12);
    f.objects.Production(sawmill).output = PRODUCER_BUFFER;
    f.Run(SawmillChain().cycleTicks * 2);
    EXPECT_EQ(f.objects.Production(sawmill).status, ProducerStatus::OutputFull);
    EXPECT_EQ(f.objects.Production(sawmill).inputs[0], PRODUCER_BUFFER); // Nothing used
}

TEST(ProductionTest, NothingWithoutARoad) {
    ProductionFixture f;
    f.SetFarmers(10);
    GameObjectId sawmill = f.AddBuilding(BUILDING_SAWMILL, { 12, 8 }); // Away from the road
    f.objects.Production(sawmill).inputs[0] = PRODUCER_BUFFER;
    f.Run(SawmillChain().cycleTicks);
    EXPECT_EQ(f.objects.Production(sawmill).status, ProducerStatus::NoRoad);
    EXPECT_EQ(f.economy.Find(ISLAND)->jobs[TIER_FARMERS], 0); // Its jobs do not count
}

TEST(ProductionTest, WorkersDoNotTakeFarmerJobs) {
    ProductionFixture f;
    f.economy.Find(ISLAND)->population[TIER_WORKERS] = 100;
    GameObjectId sawmill = f.AddSawmill(12);
    f.Run(SawmillChain().cycleTicks);
    EXPECT_EQ(f.objects.Production(sawmill).status, ProducerStatus::NoWorkforce);
    EXPECT_EQ(f.economy.Find(ISLAND)->workforce[TIER_FARMERS], 0);
}

TEST(ProductionTest, LumberjackFellsATreePerCycle) {
    ProductionFixture f;
    f.SetFarmers(10);
    // A lumberjack in the real forest nearest the spawn (connected by hand: no roads needed)
    glm::ivec2 spawn = TestWorld::Get().spawnColumn;
    glm::ivec2 forest(0);
    bool found = false;
    for (int ring = 0; ring < 20 && !found; ring++) {
        for (int dz = -ring; dz <= ring && !found; dz++) {
            for (int dx = -ring; dx <= ring && !found; dx++) {
                glm::ivec2 corner = spawn + glm::ivec2(dx, dz) * 96;
                if (f.trees.CountStanding(corner, corner + 95) >= 3) {
                    forest = corner + 48;
                    found = true;
                }
            }
        }
    }
    ASSERT_TRUE(found);
    glm::ivec2 tile(ColumnToTile(forest.x), ColumnToTile(forest.y));
    GameObjectId lumberjack = f.AddBuilding(BUILDING_LUMBERJACK, tile);
    f.objects.Logistics(lumberjack).connected = true;

    f.Run(1, false);
    const ProductionComponent& production = f.objects.Production(lumberjack);
    ASSERT_GT(production.locationFactor, 0);
    ASSERT_EQ(production.status, ProducerStatus::Working);
    int cycleTicks = PRODUCTION_CHAINS[BUILDING_TYPES[BUILDING_LUMBERJACK].chain].cycleTicks;
    f.Run(cycleTicks * 1000 / production.productivity + 2, false);
    EXPECT_EQ(production.cycles, 1u);
    EXPECT_EQ(f.trees.FelledCount(), 1u);
}
