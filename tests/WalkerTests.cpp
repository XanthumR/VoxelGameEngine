#include "Economy/IslandEconomy.h"
#include "Gameplay/Walkers.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Placement.h"
#include "Simulation/RoadNetwork.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

constexpr IslandId ISLAND = 1;

// One settled island: a farmer house at (12, 2) next to a road along z = 1
struct WalkerFixture {
    GameObjectRegistry objects;
    RoadNetwork roads;
    IslandEconomyManager economy;
    WalkerSystem walkers;
    GameObjectId house = INVALID_GAME_OBJECT;

    WalkerFixture() {
        economy.OnWarehouseAdded(ISLAND);
        for (int x = 0; x <= 30; x++) roads.Add({ x, 1 });
        for (int z = 2; z <= 10; z++) roads.Add({ 20, z }); // A side street
        house = objects.Create();
        objects.Building(house).type = BUILDING_FARMER_HOUSE;
        objects.Building(house).island = ISLAND;
        objects.Anchor(house).origin = glm::ivec3(12 * TILE_SIZE, BUILD_GROUND_Y, 2 * TILE_SIZE);
        objects.Anchor(house).footprint = glm::ivec2(3, 3) * TILE_SIZE;
    }

    void SetPopulation(int residents) {
        objects.Residence(house).residents = (uint8_t)std::min(residents, 10);
        economy.Find(ISLAND)->population[TIER_FARMERS] = residents;
    }

    void Run(float seconds) {
        for (float t = 0.0f; t < seconds; t += 0.1f) walkers.Update(0.1f, objects, roads, economy);
    }
};

} // namespace

TEST(WalkerTest, NobodyWalksWithoutResidents) {
    WalkerFixture f;
    f.Run(10.0f);
    EXPECT_EQ(f.walkers.Count(), 0u);
}

TEST(WalkerTest, OneWalkerPerFiveResidents) {
    WalkerFixture f;
    f.SetPopulation(20);
    f.Run(10.0f);
    EXPECT_EQ(f.walkers.Count(), 4u);
    EXPECT_EQ(f.walkers.CountOn(ISLAND), 4u);
    EXPECT_EQ(f.walkers.Figures().size(), 4u);
}

TEST(WalkerTest, WalkersLeaveHousesOneAtATime) {
    WalkerFixture f;
    f.SetPopulation(50);
    f.walkers.Update(0.1f, f.objects, f.roads, f.economy);
    EXPECT_EQ(f.walkers.Count(), 1u);
}

TEST(WalkerTest, FiguresStayOnTheRoad) {
    WalkerFixture f;
    f.SetPopulation(40);
    for (int step = 0; step < 600; step++) {
        f.walkers.Update(0.1f, f.objects, f.roads, f.economy);
        for (const Figure& figure : f.walkers.Figures()) {
            glm::ivec2 tile(ColumnToTile(figure.position.x), ColumnToTile(figure.position.z));
            ASSERT_TRUE(f.roads.IsRoad(tile)) << "step " << step;
            ASSERT_EQ(figure.position.y, BUILD_GROUND_Y);
        }
    }
}

TEST(WalkerTest, FewerResidentsFewerWalkers) {
    WalkerFixture f;
    f.SetPopulation(40);
    f.Run(10.0f);
    ASSERT_EQ(f.walkers.Count(), 8u);
    f.SetPopulation(10);
    f.Run(0.1f);
    EXPECT_EQ(f.walkers.Count(), 2u);
}

TEST(WalkerTest, RemovedRoadRemovesItsWalkers) {
    WalkerFixture f;
    f.SetPopulation(40);
    f.Run(10.0f);
    for (int x = 0; x <= 30; x++) f.roads.Remove({ x, 1 });
    for (int z = 2; z <= 10; z++) f.roads.Remove({ 20, z });
    f.Run(0.1f);
    EXPECT_EQ(f.walkers.Count(), 0u); // And no house has a road to leave from
}

TEST(WalkerTest, HouseWithoutRoadSendsNobody) {
    WalkerFixture f;
    f.objects.Anchor(f.house).origin = glm::ivec3(12 * TILE_SIZE, BUILD_GROUND_Y, 40 * TILE_SIZE); // Far from any road
    f.SetPopulation(30);
    f.Run(10.0f);
    EXPECT_EQ(f.walkers.Count(), 0u);
}

TEST(WalkerTest, PackedLookRoundTrips) {
    int packed = Figure::PackPerson(1, 3, 2, 5);
    EXPECT_EQ(packed & 3, 1);
    EXPECT_EQ((packed >> 2) & 3, 3);
    EXPECT_EQ((packed >> 4) & 3, 2);
    EXPECT_EQ((packed >> 6) & 7, 5);
}

TEST(WalkerTest, FiguresFaceTheWayTheyWalk) {
    WalkerFixture f;
    f.SetPopulation(5); // One walker, easy to follow
    f.Run(1.0f);
    ASSERT_EQ(f.walkers.Figures().size(), 1u);
    int checked = 0;
    for (int step = 0; step < 300; step++) {
        glm::ivec4 before = f.walkers.Figures()[0].position;
        f.walkers.Update(0.1f, f.objects, f.roads, f.economy);
        ASSERT_EQ(f.walkers.Figures().size(), 1u);
        glm::ivec4 after = f.walkers.Figures()[0].position;
        glm::ivec2 moved(after.x - before.x, after.z - before.z);
        if (moved.x != 0 && moved.y != 0) continue; // Turning a corner this step
        int direction = (after.w >> 2) & 3;
        if (moved.x > 0) { EXPECT_EQ(direction, 0); checked++; }
        if (moved.x < 0) { EXPECT_EQ(direction, 1); checked++; }
        if (moved.y > 0) { EXPECT_EQ(direction, 2); checked++; }
        if (moved.y < 0) { EXPECT_EQ(direction, 3); checked++; }
    }
    EXPECT_GT(checked, 100);
}

TEST(WalkerTest, WalkCycleGoesThroughEveryFrame) {
    WalkerFixture f;
    f.SetPopulation(5);
    bool seen[4] = {};
    for (int step = 0; step < 100; step++) {
        f.walkers.Update(0.1f, f.objects, f.roads, f.economy);
        for (const Figure& figure : f.walkers.Figures()) seen[(figure.position.w >> 4) & 3] = true;
    }
    EXPECT_TRUE(seen[0] && seen[1] && seen[2] && seen[3]);
}
