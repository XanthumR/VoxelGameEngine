#include "Economy/IslandEconomy.h"
#include "Economy/Treasury.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/Placement.h"
#include "Simulation/RoadNetwork.h"
#include "Simulation/Ships.h"
#include "TestWorld.h"

#include <gtest/gtest.h>

#include <cmath>

namespace {

using TileKind = TerrainGenerator::TileKind;

// A sea tile far enough out, searching outward from the spawn in one direction
glm::ivec2 SeaTile(glm::ivec2 direction) {
    TestWorld& test = TestWorld::Get();
    glm::ivec2 spawn(ColumnToTile(test.spawnColumn.x), ColumnToTile(test.spawnColumn.y));
    for (int ring = 1; ring < 400; ring++) {
        glm::ivec2 tile = spawn + direction * ring;
        if (test.terrain.TileKindAt(tile.x, tile.y) == TileKind::Sea && test.terrain.TileKindAt(tile.x + direction.x * 3, tile.y + direction.y * 3) == TileKind::Sea) {
            return tile + direction * 3;
        }
    }
    return spawn;
}

// Every waypoint and every point between them is open water
void ExpectAllWater(ShipSystem& ships, const OccupancyGrid& occupancy, glm::vec2 start, const std::vector<glm::ivec2>& path) {
    glm::vec2 from = start;
    for (size_t i = 0; i < path.size(); i++) {
        glm::vec2 to = glm::vec2(path[i]) + 0.5f;
        bool last = i + 1 == path.size();
        for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
            glm::ivec2 tile = glm::ivec2(glm::floor(glm::mix(from, to, t)));
            if (last && tile == path[i]) continue;
            if (i == 0 && t == 0.0f) continue;
            EXPECT_TRUE(ships.IsWater(tile, occupancy)) << "leg " << i << " at " << tile.x << "," << tile.y;
        }
        from = to;
    }
}

// A real harbor at the TestWorld coast, as an object on the occupancy grid
struct HarborFixture {
    GameObjectRegistry objects;
    OccupancyGrid occupancy;
    RoadNetwork roads;
    IslandEconomyManager economy;
    Treasury treasury;
    IslandRegistry islands{ TestWorld::Get().terrain };
    ShipSystem ships{ TestWorld::Get().terrain };
    GameObjectId harbor = INVALID_GAME_OBJECT;

    HarborFixture() { harbor = AddHarbor(0); }

    // A harbor on another coast of the spawn island: from the spawn out to sea in the next free
    // direction until one fits. island 0 = the real island's ID; others pretend it is another island.
    GameObjectId AddHarbor(IslandId island) {
        TestWorld& test = TestWorld::Get();
        PlacementContext context{ test.world, islands, occupancy, roads };
        glm::ivec2 spawn(ColumnToTile(test.spawnColumn.x), ColumnToTile(test.spawnColumn.y));
        for (glm::ivec2 direction : { glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1) }) {
            for (int ring = 0; ring < 120; ring++) {
                glm::ivec2 tile = spawn + direction * ring;
                test.LoadAround(tile * TILE_SIZE, 2);
                for (uint8_t rotation = 0; rotation < 4; rotation++) {
                    PlacementCheck check = ValidatePlacement(context, BUILDING_HARBOR, rotation, tile);
                    if (check.error != PlacementError::None) continue;
                    GameObjectId id = objects.Create();
                    objects.Building(id).type = BUILDING_HARBOR;
                    objects.Building(id).rotation = rotation;
                    objects.Building(id).island = island == 0 ? check.island : island;
                    glm::ivec2 tiles = FootprintTiles(BUILDING_TYPES[BUILDING_HARBOR], rotation);
                    objects.Anchor(id).origin = glm::ivec3(tile.x * TILE_SIZE, BUILD_GROUND_Y, tile.y * TILE_SIZE);
                    objects.Anchor(id).footprint = tiles * TILE_SIZE;
                    occupancy.Occupy(tile, tiles, id);
                    economy.OnWarehouseAdded(objects.Building(id).island);
                    return id;
                }
            }
        }
        return INVALID_GAME_OBJECT;
    }

    IslandStorage& Storage() { return *economy.Find(objects.Building(harbor).island); }

    void RunUntil(ShipId id, ShipState state, int maxTicks = 20000) {
        for (int i = 0; i < maxTicks && ships.Get(id).state != state; i++) ships.Update(objects, occupancy, economy);
    }

    void Run(int ticks) {
        for (int i = 0; i < ticks; i++) ships.Update(objects, occupancy, economy);
    }
};

} // namespace

TEST(ShipTest, PathAroundTheIslandStaysOnWater) {
    ShipSystem ships(TestWorld::Get().terrain);
    OccupancyGrid occupancy;
    glm::ivec2 east = SeaTile({ 1, 0 }), west = SeaTile({ -1, 0 });
    std::vector<glm::ivec2> path;
    ASSERT_TRUE(ships.FindPath(east, west, occupancy, path));
    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path.back(), west);
    ExpectAllWater(ships, occupancy, glm::vec2(east) + 0.5f, path);
    // Straightened: far fewer waypoints than tiles travelled
    EXPECT_LT(path.size(), (size_t)std::abs(east.x - west.x) / 2);
}

TEST(ShipTest, NoPathOntoLand) {
    HarborFixture f;
    ASSERT_NE(f.harbor, INVALID_GAME_OBJECT);
    f.treasury.SetCoins(10000);
    f.Storage().amounts[(size_t)ItemType::Planks] = 100;
    ShipId ship = f.ships.Build(f.harbor, f.objects, f.economy, f.treasury);
    ASSERT_NE(ship, INVALID_SHIP);
    glm::ivec2 spawn(ColumnToTile(TestWorld::Get().spawnColumn.x), ColumnToTile(TestWorld::Get().spawnColumn.y));
    EXPECT_FALSE(f.ships.SailTo(ship, spawn, f.objects, f.occupancy)); // The middle of the island
    EXPECT_EQ(f.ships.Get(ship).state, ShipState::Docked);           // Still where it was
}

TEST(ShipTest, BuiltAtTheHarborSailsOutAndDocksAgain) {
    HarborFixture f;
    ASSERT_NE(f.harbor, INVALID_GAME_OBJECT);
    f.treasury.SetCoins(ShipSystem::SHIP_COINS);
    f.Storage().amounts[(size_t)ItemType::Planks] = ShipSystem::SHIP_PLANKS;
    ShipId ship = f.ships.Build(f.harbor, f.objects, f.economy, f.treasury);
    ASSERT_NE(ship, INVALID_SHIP);
    EXPECT_EQ(f.treasury.Coins(), 0);
    EXPECT_EQ(f.Storage().Amount(ItemType::Planks), 0);
    EXPECT_EQ(f.ships.Get(ship).state, ShipState::Docked);
    EXPECT_EQ(glm::ivec2(glm::floor(f.ships.Get(ship).position)), ShipSystem::BerthTile(f.objects, f.harbor));
    EXPECT_EQ(f.ships.Build(f.harbor, f.objects, f.economy, f.treasury), INVALID_SHIP); // Nothing left to pay with

    // Out to open sea a few tiles from the berth, then back
    glm::ivec2 berth = ShipSystem::BerthTile(f.objects, f.harbor);
    glm::ivec2 target = berth;
    for (int r = 6; r < 40 && target == berth; r++) {
        for (glm::ivec2 d : { glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1) }) {
            if (f.ships.IsWater(berth + d * r, f.occupancy)) {
                target = berth + d * r;
                break;
            }
        }
    }
    ASSERT_NE(target, berth);
    ASSERT_TRUE(f.ships.SailTo(ship, target, f.objects, f.occupancy));
    EXPECT_EQ(f.ships.Get(ship).state, ShipState::Sailing);
    f.RunUntil(ship, ShipState::Idle);
    ASSERT_EQ(f.ships.Get(ship).state, ShipState::Idle);
    EXPECT_LT(glm::length(f.ships.Get(ship).position - (glm::vec2(target) + 0.5f)), 0.01f);

    ASSERT_TRUE(f.ships.SailTo(ship, glm::ivec2(0), f.objects, f.occupancy, f.harbor));
    f.RunUntil(ship, ShipState::Docked);
    EXPECT_EQ(f.ships.Get(ship).state, ShipState::Docked);
    EXPECT_EQ(f.ships.Get(ship).harbor, f.harbor);
}

TEST(ShipTest, CargoMovesOnlyWhileDockedAndWithinLimits) {
    HarborFixture f;
    ASSERT_NE(f.harbor, INVALID_GAME_OBJECT);
    f.treasury.SetCoins(10000);
    f.Storage().amounts.fill(0);
    f.Storage().amounts[(size_t)ItemType::Planks] = 100;
    ShipId ship = f.ships.Build(f.harbor, f.objects, f.economy, f.treasury);
    ASSERT_NE(ship, INVALID_SHIP);
    f.Storage().amounts[(size_t)ItemType::Fish] = 30;

    EXPECT_EQ(f.ships.Transfer(ship, ItemType::Planks, 70, f.objects, f.economy), ShipSystem::SLOT_CAPACITY); // A slot holds 50
    EXPECT_EQ(f.ships.Transfer(ship, ItemType::Fish, 40, f.objects, f.economy), 30);                         // All the fish there is
    EXPECT_EQ(f.ships.Transfer(ship, ItemType::Wool, 10, f.objects, f.economy), 0);                          // Both slots in use
    EXPECT_EQ(f.ships.Transfer(ship, ItemType::Fish, -10, f.objects, f.economy), -10);
    EXPECT_EQ(f.Storage().Amount(ItemType::Fish), 10);
    EXPECT_EQ(f.ships.Get(ship).cargo[1].amount, 20);

    // At sea nothing moves
    glm::ivec2 berth = ShipSystem::BerthTile(f.objects, f.harbor);
    for (int r = 6; r < 40 && f.ships.Get(ship).state == ShipState::Docked; r++) {
        for (glm::ivec2 d : { glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1) }) {
            if (f.ships.IsWater(berth + d * r, f.occupancy) && f.ships.SailTo(ship, berth + d * r, f.objects, f.occupancy)) break;
        }
    }
    EXPECT_EQ(f.ships.Transfer(ship, ItemType::Fish, -10, f.objects, f.economy), 0);
}

namespace {

// Two harbors on different coasts, pretending to be on two islands (5001 and 5002)
struct TwoHarbors : HarborFixture {
    GameObjectId first = INVALID_GAME_OBJECT, second = INVALID_GAME_OBJECT;
    ShipId ship = INVALID_SHIP;

    TwoHarbors() {
        objects.Building(harbor).island = 5001;
        economy.OnWarehouseAdded(5001);
        first = harbor;
        second = AddHarbor(5002);
        treasury.SetCoins(10000);
        Storage(first).amounts.fill(0);
        Storage(second).amounts.fill(0);
        Storage(first).amounts[(size_t)ItemType::Planks] = ShipSystem::SHIP_PLANKS;
        ship = ships.Build(first, objects, economy, treasury);
        Storage(first).amounts[(size_t)ItemType::Planks] = 40; // After paying for the ship
    }

    IslandStorage& Storage(GameObjectId building) { return *economy.Find(objects.Building(building).island); }
};

} // namespace

TEST(TradeRouteTest, CarriesGoodsBetweenTwoHarbors) {
    TwoHarbors f;
    ASSERT_NE(f.second, INVALID_GAME_OBJECT);
    ASSERT_NE(f.ship, INVALID_SHIP);
    f.Storage(f.first).amounts[(size_t)ItemType::Planks] = 40;
    f.Storage(f.first).amounts[(size_t)ItemType::Fish] = 0;
    f.Storage(f.second).amounts[(size_t)ItemType::Fish] = 25;

    int route = f.ships.CreateRoute();
    ASSERT_GE(route, 0);
    TradeRoute& r = f.ships.Route(route);
    r.stopCount = 2;
    r.stops[0].harbor = f.first;
    r.stops[0].actions[(size_t)ItemType::Planks] = StopAction::Load;
    r.stops[0].actions[(size_t)ItemType::Fish] = StopAction::Unload;
    r.stops[1].harbor = f.second;
    r.stops[1].actions[(size_t)ItemType::Planks] = StopAction::Unload;
    r.stops[1].actions[(size_t)ItemType::Fish] = StopAction::Load;
    f.ships.AssignRoute(f.ship, route);

    // Planks over, fish back: a whole loop
    for (int i = 0; i < 40000 && f.Storage(f.first).Amount(ItemType::Fish) < 25; i++) f.Run(1);
    EXPECT_EQ(f.Storage(f.second).Amount(ItemType::Planks), 40);
    EXPECT_EQ(f.Storage(f.first).Amount(ItemType::Planks), 0);
    EXPECT_EQ(f.Storage(f.first).Amount(ItemType::Fish), 25);
    EXPECT_EQ(f.Storage(f.second).Amount(ItemType::Fish), 0);
    EXPECT_EQ(f.ships.Get(f.ship).route, route); // Still on it
}

TEST(TradeRouteTest, WaitsAtAFullHarborThenUnloads) {
    TwoHarbors f;
    ASSERT_NE(f.second, INVALID_GAME_OBJECT);
    int capacity = f.Storage(f.second).CapacityPerItem();
    f.Storage(f.second).amounts[(size_t)ItemType::Planks] = capacity; // No room
    int route = f.ships.CreateRoute();
    TradeRoute& r = f.ships.Route(route);
    r.stopCount = 2;
    r.stops[0].harbor = f.first;
    r.stops[0].actions[(size_t)ItemType::Planks] = StopAction::Load;
    r.stops[1].harbor = f.second;
    r.stops[1].actions[(size_t)ItemType::Planks] = StopAction::Unload;
    f.ships.AssignRoute(f.ship, route);

    for (int i = 0; i < 40000 && !(f.ships.Get(f.ship).state == ShipState::Docked && f.ships.Get(f.ship).harbor == f.second); i++) f.Run(1);
    f.Run(ShipSystem::STOP_TICKS * 10);
    EXPECT_EQ(f.ships.Get(f.ship).state, ShipState::Docked); // Waiting with its planks
    EXPECT_EQ(f.ships.Get(f.ship).harbor, f.second);
    EXPECT_EQ(f.ships.Get(f.ship).cargo[0].amount, 40);

    f.Storage(f.second).amounts[(size_t)ItemType::Planks] = 0; // Room again
    f.Run(2);
    EXPECT_EQ(f.Storage(f.second).Amount(ItemType::Planks), 40);
    EXPECT_EQ(f.ships.Get(f.ship).cargo[0].amount, 0);
}

TEST(TradeRouteTest, StopWithoutAHarborIsSkipped) {
    TwoHarbors f;
    ASSERT_NE(f.second, INVALID_GAME_OBJECT);
    int route = f.ships.CreateRoute();
    TradeRoute& r = f.ships.Route(route);
    r.stopCount = 2;
    r.stops[0].harbor = f.second;
    r.stops[1].harbor = f.first;
    f.objects.Destroy(f.second); // Demolished
    f.ships.AssignRoute(f.ship, route);
    f.Run(ShipSystem::STOP_TICKS * 4);
    EXPECT_EQ(f.ships.Get(f.ship).state, ShipState::Docked); // Still at the harbor that is left
    EXPECT_EQ(f.ships.Get(f.ship).harbor, f.first);

    f.ships.DeleteRoute(route);
    EXPECT_EQ(f.ships.Get(f.ship).route, -1);
}
