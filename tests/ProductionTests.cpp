#include "Economy/IslandEconomy.h"
#include "Economy/Production.h"
#include "Economy/ProductionChains.h"
#include "Gameplay/Carts.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/Logistics.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/Placement.h"
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
    IslandStorage& Storage() { return *economy.Find(ISLAND); }
    void Stock(ItemType item, int amount) { Storage().amounts[(size_t)item] = amount; }

    // Runs until the producer's cart is in this state; false if it never gets there
    bool RunUntilCart(GameObjectId id, CartState state, int maxTicks = 2000) {
        for (int i = 0; i < maxTicks; i++) {
            if (objects.Production(id).cartState == state) return true;
            Run(1);
        }
        return objects.Production(id).cartState == state;
    }

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
    f.Stock(ItemType::Wood, 0); // No cart brings any
    GameObjectId sawmill = f.AddSawmill(12, 0);
    f.Run(SawmillChain().cycleTicks * 2);
    EXPECT_EQ(f.objects.Production(sawmill).status, ProducerStatus::MissingInput);
    EXPECT_EQ(f.objects.Production(sawmill).progress, 0);
    EXPECT_EQ(f.objects.Production(sawmill).output, 0);
}

TEST(ProductionTest, StopsWhenTheOutputIsFull) {
    ProductionFixture f;
    f.SetFarmers(10);
    GameObjectId sawmill = f.AddBuilding(BUILDING_SAWMILL, { 12, 8 }); // No road: no cart takes the output
    f.objects.Production(sawmill).inputs[0] = PRODUCER_BUFFER;
    f.objects.Production(sawmill).output = PRODUCER_BUFFER;
    f.objects.Logistics(sawmill).connected = true; // Pretend it is connected
    f.Run(SawmillChain().cycleTicks * 2, false);
    EXPECT_EQ(f.objects.Production(sawmill).status, ProducerStatus::OutputFull);
    EXPECT_EQ(f.objects.Production(sawmill).inputs[0], PRODUCER_BUFFER); // Nothing used
    EXPECT_EQ(f.objects.Production(sawmill).cartState, CartState::Idle);
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

TEST(CartTest, RouteRunsDownhillToTheWarehouse) {
    ProductionFixture f;
    GameObjectId sawmill = f.AddSawmill(12);
    f.Run(1);
    ProductionComponent& production = f.objects.Production(sawmill);
    ASSERT_TRUE(ProductionSystem::FindCartPath(f.roads, { 12, 2 }, { 3, 3 }, production));
    ASSERT_GT(production.cartPathLength, 1);
    EXPECT_EQ(f.roads.Find(production.cartPath[production.cartPathLength - 1])->distance, 1); // Touches the warehouse
    for (int i = 1; i < production.cartPathLength; i++) {
        glm::ivec2 step = production.cartPath[i] - production.cartPath[i - 1];
        EXPECT_EQ(std::abs(step.x) + std::abs(step.y), 1);
        EXPECT_EQ(f.roads.Find(production.cartPath[i])->distance, f.roads.Find(production.cartPath[i - 1])->distance - 1);
    }
    EXPECT_FALSE(ProductionSystem::FindCartPath(f.roads, { 12, 8 }, { 3, 3 }, production)); // No road around it
}

TEST(CartTest, FullLoadGoesToTheWarehouseAndBringsInputsBack) {
    ProductionFixture f;
    f.Stock(ItemType::Planks, 0);
    f.Stock(ItemType::Wood, 20);
    GameObjectId sawmill = f.AddSawmill(12, 1);
    f.objects.Production(sawmill).output = CART_CAPACITY; // No workers: nothing else changes
    f.Run(1);
    const ProductionComponent& production = f.objects.Production(sawmill);
    ASSERT_EQ(production.cartState, CartState::ToWarehouse); // Leaves at once with a full load
    EXPECT_EQ(production.cartOutput, CART_CAPACITY);
    EXPECT_EQ(production.output, 0);

    // Drives the route at CART_SPEED
    int driveTicks = ((production.cartPathLength - 1) * CART_TILE + CART_SPEED - 1) / CART_SPEED;
    f.Run(driveTicks - 1);
    EXPECT_EQ(production.cartState, CartState::ToWarehouse);
    f.Run(1);
    EXPECT_EQ(production.cartState, CartState::Unloading);

    ASSERT_TRUE(f.RunUntilCart(sawmill, CartState::ToProducer, CART_UNLOAD_TICKS + 2));
    EXPECT_EQ(f.Storage().Amount(ItemType::Planks), CART_CAPACITY);
    EXPECT_EQ(production.cartInputs[0], PRODUCER_BUFFER - 1); // What the sawmill lacked
    EXPECT_EQ(f.Storage().Amount(ItemType::Wood), 20 - (PRODUCER_BUFFER - 1));

    f.Run(driveTicks - 1);
    EXPECT_EQ(production.cartState, CartState::ToProducer);
    f.Run(1);
    EXPECT_EQ(production.cartState, CartState::Idle);
    EXPECT_EQ(production.inputs[0], PRODUCER_BUFFER);
    EXPECT_EQ(production.cartInputs[0], 0);
}

TEST(CartTest, WaitsForAFullLoadThenGoesAnyway) {
    ProductionFixture f;
    f.Stock(ItemType::Wood, 0); // Nothing to fetch
    GameObjectId sawmill = f.AddSawmill(12);
    f.objects.Production(sawmill).output = 1;
    f.Run(CART_MAX_WAIT_TICKS - 1);
    EXPECT_EQ(f.objects.Production(sawmill).cartState, CartState::Idle);
    f.Run(1);
    EXPECT_EQ(f.objects.Production(sawmill).cartState, CartState::ToWarehouse);
    EXPECT_EQ(f.objects.Production(sawmill).cartOutput, 1);
}

TEST(CartTest, StarvedProducerFetchesAtOnce) {
    ProductionFixture f;
    f.Stock(ItemType::Wood, 20);
    GameObjectId sawmill = f.AddSawmill(12, 0);
    f.Run(1);
    EXPECT_EQ(f.objects.Production(sawmill).cartState, CartState::ToWarehouse);
    EXPECT_EQ(f.objects.Production(sawmill).cartOutput, 0);
    ASSERT_TRUE(f.RunUntilCart(sawmill, CartState::Idle));
    EXPECT_EQ(f.objects.Production(sawmill).inputs[0], PRODUCER_BUFFER);
}

TEST(CartTest, NothingToCarryStaysHome) {
    ProductionFixture f;
    f.Stock(ItemType::Wood, 0);
    GameObjectId sawmill = f.AddSawmill(12, 0);
    f.Run(CART_MAX_WAIT_TICKS * 2);
    EXPECT_EQ(f.objects.Production(sawmill).cartState, CartState::Idle);
    EXPECT_EQ(f.production.CartsOnRoad(), 0u);
}

TEST(CartTest, CutRoadSendsTheCartHomeWithItsCargo) {
    ProductionFixture f;
    f.Stock(ItemType::Planks, 0);
    GameObjectId sawmill = f.AddSawmill(12);
    f.objects.Production(sawmill).output = CART_CAPACITY;
    f.Run(1);
    const ProductionComponent& production = f.objects.Production(sawmill);
    ASSERT_EQ(production.cartState, CartState::ToWarehouse);
    ASSERT_GE(production.cartPathLength, 4);
    f.Run(CART_TILE / CART_SPEED + 1); // On its way
    EXPECT_EQ(f.production.CartsOnRoad(), 1u);

    // Cut the tile it is driving to while it is between two tiles: it turns around there
    int gap = production.cartPosition / CART_TILE + 1;
    ASSERT_NE(production.cartPosition % CART_TILE, 0);
    f.roads.Remove(production.cartPath[gap]);
    f.Run(1, false);
    ASSERT_EQ(production.cartState, CartState::ToProducer);
    EXPECT_LT(production.cartPosition, gap * CART_TILE);

    // It drives back the whole way, not straight home
    int turnedAt = production.cartPosition;
    int driveHome = (turnedAt + CART_SPEED - 1) / CART_SPEED;
    f.Run(driveHome - 1);
    EXPECT_EQ(production.cartState, CartState::ToProducer);
    EXPECT_EQ(production.cartPosition, turnedAt - (driveHome - 1) * CART_SPEED);
    f.Run(1);
    EXPECT_EQ(production.cartState, CartState::Idle);
    EXPECT_EQ(production.output, CART_CAPACITY); // The cargo is back
    EXPECT_EQ(f.Storage().Amount(ItemType::Planks), 0);
}

TEST(CartTest, WaitsAtAFullWarehouse) {
    ProductionFixture f;
    GameObjectId sawmill = f.AddSawmill(12);
    f.Stock(ItemType::Planks, f.Storage().CapacityPerItem());
    f.objects.Production(sawmill).output = CART_CAPACITY;
    ASSERT_TRUE(f.RunUntilCart(sawmill, CartState::Unloading));
    f.Run(CART_UNLOAD_TICKS * 5);
    EXPECT_EQ(f.objects.Production(sawmill).cartState, CartState::Unloading);
    EXPECT_EQ(f.objects.Production(sawmill).cartOutput, CART_CAPACITY);

    f.Stock(ItemType::Planks, 0); // Room again
    f.Run(1);
    EXPECT_EQ(f.objects.Production(sawmill).cartState, CartState::ToProducer);
    EXPECT_EQ(f.Storage().Amount(ItemType::Planks), CART_CAPACITY);
}

TEST(CartTest, ProducedGoodsReachIslandStorage) {
    ProductionFixture f;
    f.SetFarmers(10);
    f.Stock(ItemType::Planks, 0);
    f.Stock(ItemType::Wood, 40);
    f.AddSawmill(12, 0);
    f.Run(SawmillChain().cycleTicks * 12);
    // About 12 cycles minus the first fetch and what is still at the sawmill or on the road
    EXPECT_GE(f.Storage().Amount(ItemType::Planks), 4);
    EXPECT_LT(f.Storage().Amount(ItemType::Wood), 40);
}

namespace {

// A cart on a straight route along +x from tile (10, 5) to (13, 5)
ProductionComponent StraightRoute(CartState state, int position) {
    ProductionComponent production;
    production.cartState = state;
    production.cartPathLength = 4;
    for (int i = 0; i < 4; i++) production.cartPath[i] = glm::ivec2(10 + i, 5);
    production.cartPosition = position;
    return production;
}

} // namespace

TEST(CartPlacementTest, HiddenAtHome) {
    EXPECT_FALSE(PlaceCart(StraightRoute(CartState::Idle, 0), 0.0f).visible);
}

TEST(CartPlacementTest, DrivesInTheRightHandLane) {
    float middleZ = 5 * TILE_SIZE + (TILE_SIZE - 1) * 0.5f;
    CartPlacement out = PlaceCart(StraightRoute(CartState::ToWarehouse, 1500), 0.0f);
    ASSERT_TRUE(out.visible);
    EXPECT_EQ(out.direction, 0);
    EXPECT_TRUE(out.moving);
    EXPECT_FLOAT_EQ(out.column.x, 11.5f * TILE_SIZE + (TILE_SIZE - 1) * 0.5f);
    EXPECT_FLOAT_EQ(out.column.y, middleZ + CART_LANE_OFFSET);

    CartPlacement back = PlaceCart(StraightRoute(CartState::ToProducer, 1500), 0.0f);
    EXPECT_EQ(back.direction, 1);
    EXPECT_FLOAT_EQ(back.column.y, middleZ - CART_LANE_OFFSET); // The other side of the road
}

TEST(CartPlacementTest, MovesOnBetweenTicks) {
    CartPlacement now = PlaceCart(StraightRoute(CartState::ToWarehouse, 1000), 0.0f);
    CartPlacement later = PlaceCart(StraightRoute(CartState::ToWarehouse, 1000), 0.5f);
    EXPECT_NEAR(later.column.x - now.column.x, 0.5f * CART_SPEED * TILE_SIZE / CART_TILE, 1e-4f);
    CartPlacement end = PlaceCart(StraightRoute(CartState::ToWarehouse, 3 * CART_TILE), 1.0f);
    EXPECT_FLOAT_EQ(end.column.x, 13 * TILE_SIZE + (TILE_SIZE - 1) * 0.5f); // Not past the route's end
    CartPlacement unloading = PlaceCart(StraightRoute(CartState::Unloading, 3 * CART_TILE), 0.7f);
    EXPECT_FALSE(unloading.moving);
    EXPECT_FLOAT_EQ(unloading.column.x, end.column.x);
}

TEST(CartPlacementTest, CartFigureForEveryCartOnTheRoad) {
    ProductionFixture f;
    f.Stock(ItemType::Planks, 0);
    GameObjectId sawmill = f.AddSawmill(12);
    f.AddSawmill(16);
    f.objects.Production(sawmill).output = CART_CAPACITY;
    f.Run(1);
    std::vector<Figure> figures;
    AppendCartFigures(f.objects, 0.0f, figures);
    ASSERT_EQ(figures.size(), 1u);
    EXPECT_EQ(Figure::KindOf(figures[0].position.w), Figure::CART);
    EXPECT_EQ(figures[0].position.y, BUILD_GROUND_Y);
    EXPECT_EQ((figures[0].position.w >> 6) & 7, CART_CAPACITY);                 // A full load
    EXPECT_EQ((figures[0].position.w >> 11) & 15, (int)ItemType::Planks);       // Of planks
}
