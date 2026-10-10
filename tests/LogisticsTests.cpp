#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Logistics.h"
#include "Simulation/RoadNetwork.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace {

// Game objects and roads only (no voxels): buildings are anchored on the tile grid directly
struct LogisticsFixture {
    GameObjectRegistry objects;
    RoadNetwork roads;
    LogisticsSystem logistics;
    uint32_t buildingsRevision = 0;

    GameObjectId AddBuilding(uint16_t type, glm::ivec2 minTile) {
        GameObjectId id = objects.Create();
        objects.Building(id).type = type;
        const BuildingType& building = BUILDING_TYPES[type];
        objects.Anchor(id).origin = glm::ivec3(minTile.x * TILE_SIZE, 0, minTile.y * TILE_SIZE);
        objects.Anchor(id).footprint = glm::ivec2(building.footprintWidth, building.footprintDepth) * TILE_SIZE;
        buildingsRevision++;
        return id;
    }

    // Straight road along x from (x0, z) to (x1, z), both included
    void AddRoadX(int x0, int x1, int z) {
        for (int x = x0; x <= x1; x++) roads.Add({ x, z });
    }

    void Run() { logistics.Update(objects, roads, buildingsRevision); }
};

} // namespace

// The warehouse is 4x4 tiles at (0,0)-(3,3); the road starts touching its right side at x = 4
TEST(LogisticsTest, DistanceGrowsAlongTheRoad) {
    LogisticsFixture f;
    GameObjectId warehouse = f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 10, 1);
    f.Run();
    EXPECT_EQ(f.roads.Find({ 4, 1 })->distance, 1);
    EXPECT_EQ(f.roads.Find({ 10, 1 })->distance, 7);
    EXPECT_EQ(f.roads.Find({ 10, 1 })->warehouse, warehouse);
}

TEST(LogisticsTest, RangeStopsAtExactlyThirtyTiles) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 4 + WAREHOUSE_ROAD_RANGE + 5, 1);
    f.Run();
    EXPECT_EQ(f.roads.Find({ 4 + WAREHOUSE_ROAD_RANGE - 1, 1 })->distance, WAREHOUSE_ROAD_RANGE);
    EXPECT_EQ(f.roads.Find({ 4 + WAREHOUSE_ROAD_RANGE, 1 })->distance, RoadTile::UNREACHED);
}

TEST(LogisticsTest, HouseOnAnInRangeRoadIsConnected) {
    LogisticsFixture f;
    GameObjectId warehouse = f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 20, 1);
    GameObjectId house = f.AddBuilding(BUILDING_FARMER_HOUSE, { 12, 2 }); // Its top side touches the road at z = 1
    f.Run();
    const LogisticsComponent& logistics = f.objects.Logistics(house);
    EXPECT_TRUE(logistics.connected);
    EXPECT_EQ(logistics.warehouse, warehouse);
    EXPECT_EQ(logistics.roadDistance, 9); // Road tile (12, 1) is 9 tiles from the warehouse
    EXPECT_TRUE(f.objects.Logistics(warehouse).connected);
    EXPECT_EQ(f.logistics.ConnectedBuildings(), 1u);
    EXPECT_EQ(f.logistics.TotalBuildings(), 1u);
}

TEST(LogisticsTest, HouseOnlyOnAnOutOfRangeRoadIsNotConnected) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 4 + WAREHOUSE_ROAD_RANGE + 10, 1);
    GameObjectId house = f.AddBuilding(BUILDING_FARMER_HOUSE, { 4 + WAREHOUSE_ROAD_RANGE + 2, 2 });
    f.Run();
    EXPECT_FALSE(f.objects.Logistics(house).connected);
}

TEST(LogisticsTest, HouseWithoutRoadIsNotConnected) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    GameObjectId house = f.AddBuilding(BUILDING_FARMER_HOUSE, { 6, 0 });
    f.Run();
    EXPECT_FALSE(f.objects.Logistics(house).connected);
    EXPECT_EQ(f.logistics.ConnectedBuildings(), 0u);
}

TEST(LogisticsTest, RemovingARoadTileBreaksTheConnection) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 20, 1);
    GameObjectId house = f.AddBuilding(BUILDING_FARMER_HOUSE, { 12, 2 });
    f.Run();
    ASSERT_TRUE(f.objects.Logistics(house).connected);

    f.roads.Remove({ 8, 1 }); // Cut between the warehouse and the house
    f.Run();
    EXPECT_FALSE(f.objects.Logistics(house).connected);
    EXPECT_EQ(f.roads.Find({ 12, 1 })->distance, RoadTile::UNREACHED);
}

TEST(LogisticsTest, NearestWarehouseWins) {
    LogisticsFixture f;
    GameObjectId west = f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    GameObjectId east = f.AddBuilding(BUILDING_WAREHOUSE, { 25, 0 });
    f.AddRoadX(4, 24, 1); // Between the two
    GameObjectId nearEast = f.AddBuilding(BUILDING_FARMER_HOUSE, { 20, 2 });
    GameObjectId nearWest = f.AddBuilding(BUILDING_FARMER_HOUSE, { 5, 2 });
    f.Run();
    EXPECT_EQ(f.objects.Logistics(nearEast).warehouse, east);
    EXPECT_EQ(f.objects.Logistics(nearWest).warehouse, west);
    EXPECT_EQ(f.roads.Find({ 24, 1 })->distance, 1);
}

TEST(LogisticsTest, OnlyRebuildsWhenSomethingChanged) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    EXPECT_TRUE(f.logistics.Update(f.objects, f.roads, f.buildingsRevision));
    EXPECT_FALSE(f.logistics.Update(f.objects, f.roads, f.buildingsRevision));
    f.roads.Add({ 4, 0 });
    EXPECT_TRUE(f.logistics.Update(f.objects, f.roads, f.buildingsRevision));
}

TEST(LogisticsTest, PreviewReachMatchesARealWarehouse) {
    LogisticsFixture f;
    f.AddRoadX(4, 4 + WAREHOUSE_ROAD_RANGE + 5, 1);
    std::vector<glm::ivec2> reach;
    f.logistics.PreviewReach(f.roads, { 0, 0 }, { 4, 4 }, WAREHOUSE_ROAD_RANGE, reach);
    EXPECT_EQ(reach.size(), (size_t)WAREHOUSE_ROAD_RANGE);
    EXPECT_NE(std::find(reach.begin(), reach.end(), glm::ivec2(4, 1)), reach.end());
    EXPECT_EQ(std::find(reach.begin(), reach.end(), glm::ivec2(4 + WAREHOUSE_ROAD_RANGE, 1)), reach.end());
}

constexpr size_t MARKET = (size_t)ServiceType::Marketplace;

// Marketplace: 4x3 tiles at (6, 2)-(9, 4), touching the road at z = 1 along x 6..9
TEST(LogisticsTest, MarketReachSpreadsFromAConnectedMarketplace) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 40, 1);
    GameObjectId market = f.AddBuilding(BUILDING_MARKETPLACE, { 6, 2 });
    f.Run();
    EXPECT_EQ(f.roads.Find({ 9, 1 })->serviceDistance[MARKET], 1);
    EXPECT_EQ(f.roads.Find({ 9, 1 })->service[MARKET], market);
    EXPECT_EQ(f.roads.Find({ 9 + MARKET_ROAD_RANGE - 1, 1 })->serviceDistance[MARKET], MARKET_ROAD_RANGE);
    EXPECT_EQ(f.roads.Find({ 9 + MARKET_ROAD_RANGE, 1 })->serviceDistance[MARKET], RoadTile::UNREACHED);
}

TEST(LogisticsTest, HouseInMarketReachIsServed) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 40, 1);
    GameObjectId market = f.AddBuilding(BUILDING_MARKETPLACE, { 6, 2 });
    GameObjectId near = f.AddBuilding(BUILDING_FARMER_HOUSE, { 12, 2 });
    GameObjectId far = f.AddBuilding(BUILDING_FARMER_HOUSE, { 9 + MARKET_ROAD_RANGE + 1, 2 });
    f.Run();
    EXPECT_TRUE(f.objects.Logistics(near).InReach(ServiceType::Marketplace));
    EXPECT_EQ(f.objects.Logistics(near).services[MARKET], market);
    EXPECT_TRUE(f.objects.Logistics(far).connected);       // Still within the warehouse's reach
    EXPECT_FALSE(f.objects.Logistics(far).InReach(ServiceType::Marketplace));  // But no marketplace reaches it
}

TEST(LogisticsTest, MarketplaceWithoutWarehouseServesNobody) {
    LogisticsFixture f;
    f.AddRoadX(4, 40, 1);
    f.AddBuilding(BUILDING_MARKETPLACE, { 6, 2 });
    GameObjectId house = f.AddBuilding(BUILDING_FARMER_HOUSE, { 12, 2 });
    f.Run();
    EXPECT_FALSE(f.objects.Logistics(house).InReach(ServiceType::Marketplace));
    EXPECT_EQ(f.roads.Find({ 9, 1 })->serviceDistance[MARKET], RoadTile::UNREACHED);
}

TEST(LogisticsTest, NearestMarketplaceWins) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 40, 1);
    GameObjectId west = f.AddBuilding(BUILDING_MARKETPLACE, { 6, 2 });
    GameObjectId east = f.AddBuilding(BUILDING_MARKETPLACE, { 26, 2 });
    GameObjectId house = f.AddBuilding(BUILDING_FARMER_HOUSE, { 22, 2 }); // Touching road 14 tiles from the west one, 3 from the east one
    f.Run();
    EXPECT_EQ(f.objects.Logistics(house).services[MARKET], east);
    (void)west;
}

TEST(LogisticsTest, EachServiceSpreadsItsOwnRange) {
    LogisticsFixture f;
    f.AddBuilding(BUILDING_WAREHOUSE, { 0, 0 });
    f.AddRoadX(4, 60, 1);
    GameObjectId school = f.AddBuilding(BUILDING_SCHOOL, { 6, 2 }); // 3x3: touches the road along x 6..8
    const size_t SCHOOL = (size_t)ServiceType::School;
    int range = SERVICE_ROAD_RANGE[SCHOOL];
    GameObjectId near = f.AddBuilding(BUILDING_FARMER_HOUSE, { 8 + range - 2, 2 });
    GameObjectId far = f.AddBuilding(BUILDING_FARMER_HOUSE, { 8 + range + 1, 2 });
    f.Run();
    EXPECT_EQ(f.roads.Find({ 8, 1 })->serviceDistance[SCHOOL], 1);
    EXPECT_EQ(f.roads.Find({ 8 + range, 1 })->serviceDistance[SCHOOL], RoadTile::UNREACHED);
    EXPECT_EQ(f.objects.Logistics(near).services[SCHOOL], school);
    EXPECT_FALSE(f.objects.Logistics(far).InReach(ServiceType::School));
    EXPECT_FALSE(f.objects.Logistics(near).InReach(ServiceType::Marketplace)); // A school is no marketplace
}

TEST(LogisticsTest, PreviewReachUsesTheGivenRange) {
    LogisticsFixture f;
    f.AddRoadX(4, 60, 1);
    std::vector<glm::ivec2> reach;
    f.logistics.PreviewReach(f.roads, { 0, 0 }, { 4, 3 }, MARKET_ROAD_RANGE, reach);
    EXPECT_EQ(reach.size(), (size_t)MARKET_ROAD_RANGE);
}
