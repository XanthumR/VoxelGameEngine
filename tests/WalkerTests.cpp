#include "Economy/IslandEconomy.h"
#include "Gameplay/FigureModels.h"
#include "Gameplay/Walkers.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Placement.h"
#include "Simulation/RoadNetwork.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

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
    EXPECT_EQ(f.walkers.Objects().size(), 4u);
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
        for (const VoxelObject& object : f.walkers.Objects()) {
            glm::ivec2 tile(ColumnToTile((int)std::floor(object.position.x)), ColumnToTile((int)std::floor(object.position.z)));
            ASSERT_TRUE(f.roads.IsRoad(tile)) << "step " << step;
            ASSERT_EQ(object.position.y, (float)BUILD_GROUND_Y);
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

TEST(WalkerTest, WalkersFaceTheWayTheyWalk) {
    WalkerFixture f;
    f.SetPopulation(5); // One walker, easy to follow
    f.Run(1.0f);
    ASSERT_EQ(f.walkers.Objects().size(), 1u);
    int checked = 0;
    glm::vec2 lastMove(0.0f);
    int straight = 0; // Steps walked the same way
    for (int step = 0; step < 300; step++) {
        glm::vec3 before = f.walkers.Objects()[0].position;
        float yawBefore = f.walkers.Objects()[0].yaw; // Facing at the start of the step it walked
        f.walkers.Update(0.1f, f.objects, f.roads, f.economy);
        ASSERT_EQ(f.walkers.Objects().size(), 1u);
        glm::vec3 after = f.walkers.Objects()[0].position;
        glm::vec2 moved(after.x - before.x, after.z - before.z);
        // Walking straight on for half a second: the turn is done and it faces the way it goes
        bool same = glm::length(moved) > 0.1f && glm::length(lastMove) > 0.1f && glm::dot(glm::normalize(moved), glm::normalize(lastMove)) > 0.99f;
        straight = same ? straight + 1 : 0;
        if (straight >= 5) {
            glm::vec2 facing(std::sin(yawBefore), std::cos(yawBefore));
            EXPECT_GT(glm::dot(facing, glm::normalize(moved)), 0.99f) << "step " << step;
            checked++;
        }
        lastMove = moved;
    }
    EXPECT_GT(checked, 100);
}

TEST(WalkerTest, WalkCycleGoesThroughEveryFrame) {
    WalkerFixture f;
    f.SetPopulation(5);
    bool seen[4] = {};
    for (int step = 0; step < 100; step++) {
        f.walkers.Update(0.1f, f.objects, f.roads, f.economy);
        for (const VoxelObject& object : f.walkers.Objects()) seen[object.model % WALK_FRAMES] = true; // Model base 0
    }
    EXPECT_TRUE(seen[0] && seen[1] && seen[2] && seen[3]);
}
