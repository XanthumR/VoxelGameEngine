#include "Gameplay/FishingBoats.h"
#include "Gameplay/Smoke.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingModels.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/Placement.h"
#include "Simulation/RoadNetwork.h"
#include "TestWorld.h"
#include "World/BlockTypes.h"
#include "World/BuildingModel.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

// ASSET_DIRECTORY is defined by the test project (the repository's assets folder)
namespace {

const std::string BUILDINGS = std::string(ASSET_DIRECTORY) + "/buildings";

const BuildingModelLibrary& Library() {
    static BuildingModelLibrary library;
    static bool loaded = false;
    if (!loaded) {
        library.LoadAll(BUILDINGS);
        loaded = true;
    }
    return library;
}

// The open sea nearest the spawn
glm::ivec2 SeaColumn() {
    TestWorld& test = TestWorld::Get();
    const glm::ivec2 directions[8] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    for (int ring = 1; ring < 200; ring++) {
        for (const glm::ivec2& direction : directions) {
            glm::ivec2 column = test.spawnColumn + direction * ring * 32;
            if (test.terrain.TerrainHeightAt(column.x, column.y) < SEA_LEVEL - 6) return column;
        }
    }
    return test.spawnColumn;
}

} // namespace

TEST(BuildingMarkerTest, ChimneysAndBerthsAreFoundAndNotDrawn) {
    BuildingModel farmer, worker, fishery;
    ASSERT_TRUE(farmer.Load(BUILDINGS + "/farmer_house_1.vox"));
    ASSERT_TRUE(worker.Load(BUILDINGS + "/worker_house_1.vox"));
    ASSERT_TRUE(fishery.Load(BUILDINGS + "/fishery_1.vox"));
    EXPECT_EQ(farmer.smokeEmitters.size(), 1u);
    EXPECT_EQ(worker.smokeEmitters.size(), 2u);
    EXPECT_EQ(fishery.boatBerths.size(), 1u);
    for (const glm::ivec3& emitter : farmer.smokeEmitters) {
        EXPECT_EQ(farmer.At(emitter.x, emitter.y, emitter.z), Block::AIR);
        EXPECT_NE(farmer.At(emitter.x + 1, emitter.y, emitter.z), Block::AIR); // Inside the chimney's rim
    }
    // The berth is at the dock's end, at the waterline
    const BuildingType& type = BUILDING_TYPES[BUILDING_FISHERY];
    EXPECT_EQ(fishery.boatBerths[0].y, fishery.depth - 1);
    EXPECT_EQ(BUILD_GROUND_Y - type.belowGround + fishery.boatBerths[0].z, SEA_LEVEL);
}

TEST(CoastalPlacementTest, FisheryDockMustReachOverTheWater) {
    TestWorld& test = TestWorld::Get();
    // Walk from the sea toward the spawn and try every rotation until a fishery fits
    glm::ivec2 sea = SeaColumn();
    test.LoadAround(sea, 6);
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    RoadNetwork roads;
    PlacementContext context{ test.world, islands, occupancy, roads };

    glm::vec2 toSpawn = glm::normalize(glm::vec2(test.spawnColumn - sea));
    for (int step = 0; step < 40; step++) {
        glm::ivec2 column = sea + glm::ivec2(toSpawn * (float)(step * 6));
        glm::ivec2 near(ColumnToTile(column.x), ColumnToTile(column.y));
        for (int dz = -2; dz <= 2; dz++) {
            for (int dx = -2; dx <= 2; dx++) {
                for (uint8_t rotation = 0; rotation < 4; rotation++) {
                    glm::ivec2 tile = near + glm::ivec2(dx, dz);
                    if (ValidatePlacement(context, BUILDING_FISHERY, rotation, tile).error != PlacementError::None) continue;
                    // Turned around, the dock faces the land
                    PlacementError turned = ValidatePlacement(context, BUILDING_FISHERY, (uint8_t)((rotation + 2) & 3), tile).error;
                    EXPECT_NE(turned, PlacementError::None);
                    return;
                }
            }
        }
    }
    FAIL() << "No coast spot for a fishery found";
}

TEST(SmokeTest, OnlyBuildingsInUseSmoke) {
    GameObjectRegistry objects;
    GameObjectId house = objects.Create();
    objects.Building(house).type = BUILDING_FARMER_HOUSE;
    objects.Anchor(house).origin = glm::ivec3(0, BUILD_GROUND_Y, 0);
    objects.Anchor(house).footprint = glm::ivec2(36, 36);

    SmokeSystem smoke;
    for (int i = 0; i < 30; i++) smoke.Update(0.1f, objects, Library(), glm::vec2(18.0f), 1000.0f);
    EXPECT_EQ(smoke.PuffCount(), 0u); // Nobody lives there

    objects.Residence(house).residents = 4;
    for (int i = 0; i < 30; i++) smoke.Update(0.1f, objects, Library(), glm::vec2(18.0f), 1000.0f);
    EXPECT_GT(smoke.PuffCount(), 0u);

    std::vector<Figure> figures;
    smoke.AppendFigures(figures);
    ASSERT_FALSE(figures.empty());
    EXPECT_EQ(Figure::KindOf(figures[0].position.w), Figure::PUFF);
    EXPECT_GT(figures[0].position.y, BUILD_GROUND_Y + 20); // Above the chimney

    objects.Residence(house).residents = 0;
    for (int i = 0; i < 60; i++) smoke.Update(0.1f, objects, Library(), glm::vec2(18.0f), 1000.0f);
    EXPECT_EQ(smoke.PuffCount(), 0u); // The last puffs have faded
}

TEST(SmokeTest, FarAwayChimneysDoNotSmoke) {
    GameObjectRegistry objects;
    GameObjectId house = objects.Create();
    objects.Building(house).type = BUILDING_FARMER_HOUSE;
    objects.Anchor(house).origin = glm::ivec3(5000, BUILD_GROUND_Y, 0);
    objects.Anchor(house).footprint = glm::ivec2(36, 36);
    objects.Residence(house).residents = 4;
    SmokeSystem smoke;
    for (int i = 0; i < 30; i++) smoke.Update(0.1f, objects, Library(), glm::vec2(0.0f), 1000.0f);
    EXPECT_EQ(smoke.PuffCount(), 0u);
}

TEST(SmokeTest, PuffCapHolds) {
    GameObjectRegistry objects;
    for (int i = 0; i < 400; i++) {
        GameObjectId house = objects.Create();
        objects.Building(house).type = BUILDING_WORKER_HOUSE;
        objects.Anchor(house).origin = glm::ivec3((i % 20) * 36, BUILD_GROUND_Y, (i / 20) * 36);
        objects.Anchor(house).footprint = glm::ivec2(36, 36);
        objects.Residence(house).residents = 20;
    }
    SmokeSystem smoke;
    for (int i = 0; i < 60; i++) smoke.Update(0.1f, objects, Library(), glm::vec2(360.0f), 5000.0f);
    EXPECT_LE(smoke.PuffCount(), (size_t)SmokeSystem::MAX_PUFFS);
    EXPECT_GT(smoke.PuffCount(), 1000u);
}

TEST(FishingBoatTest, TripStartsAndEndsAtTheBerth) {
    EXPECT_EQ(FishingBoats::OffsetAt(0.0f, 60), 0.0f);
    EXPECT_EQ(FishingBoats::OffsetAt(FishingBoats::MOORED_SECONDS * 0.5f, 60), 0.0f);
    EXPECT_FLOAT_EQ(FishingBoats::OffsetAt(FishingBoats::TRIP_SECONDS * 0.5f, 60), 60.0f); // Out fishing
    EXPECT_NEAR(FishingBoats::OffsetAt(FishingBoats::TRIP_SECONDS - 0.001f, 60), 0.0f, 0.1f);
    float previous = 0.0f;
    for (float t = 0.0f; t < FishingBoats::TRIP_SECONDS * 0.5f; t += 0.5f) { // Never goes past the route
        float offset = FishingBoats::OffsetAt(t, 60);
        EXPECT_GE(offset, previous);
        EXPECT_LE(offset, 60.0f);
        previous = offset;
    }
}

TEST(FishingBoatTest, RouteNeverReachesLand) {
    TestWorld& test = TestWorld::Get();
    glm::ivec2 sea = SeaColumn();
    glm::ivec2 toSpawn = glm::ivec2(glm::sign(glm::vec2(test.spawnColumn - sea)));
    if (toSpawn.x != 0 && toSpawn.y != 0) toSpawn.y = 0; // Along an axis, as boats sail
    // Sailing toward the island from open sea: the route stops before the shore
    int length = FishingBoats::RouteLength(test.terrain, sea, toSpawn, 2000);
    EXPECT_LT(length, 2000);
    for (int step = 0; step <= length + FishingBoats::BOAT_HALF_LENGTH; step++) {
        glm::ivec2 column = sea + toSpawn * step;
        EXPECT_LE(test.terrain.TerrainHeightAt(column.x, column.y), SEA_LEVEL - 1) << "step " << step;
    }
}
