#include "Simulation/BuildingLook.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace {

uint8_t At(const std::vector<uint8_t>& ids, glm::ivec2 size, int x, int y, int z) {
    return ids[(size_t)x + (size_t)size.x * ((size_t)z + (size_t)size.y * (size_t)y)];
}

} // namespace

TEST(BuildingLookTest, FootprintSwapsForSidewaysRotations) {
    BuildingType wide{ "Test", 2, 3, 4, 10, Block::PLANK, Block::ROOF, BuildingRole::Residence, 0, LookStyle::Gable, true, nullptr, BuildCategory::Housing, -1 };
    EXPECT_EQ(FootprintTiles(wide, 0), glm::ivec2(2, 3));
    EXPECT_EQ(FootprintTiles(wide, 1), glm::ivec2(3, 2));
    EXPECT_EQ(FootprintTiles(wide, 2), glm::ivec2(2, 3));
    EXPECT_EQ(FootprintTiles(wide, 3), glm::ivec2(3, 2));
    EXPECT_EQ(FootprintColumns(wide, 1), glm::ivec2(3, 2) * TILE_SIZE);
}

TEST(BuildingLookTest, VolumeMatchesFootprintAndHeight) {
    for (const BuildingType& type : BUILDING_TYPES) {
        for (uint8_t rotation = 0; rotation < 4; rotation++) {
            std::vector<uint8_t> ids;
            BuildLook(type, rotation, ids);
            glm::ivec2 size = FootprintColumns(type, rotation);
            EXPECT_EQ(ids.size(), (size_t)size.x * size.y * BuildingHeight(type)) << type.name;
        }
    }
}

TEST(BuildingLookTest, RotationKeepsTheSameBlocks) {
    for (const BuildingType& type : BUILDING_TYPES) {
        std::vector<uint8_t> base;
        BuildLook(type, 0, base);
        std::sort(base.begin(), base.end());
        for (uint8_t rotation = 1; rotation < 4; rotation++) {
            std::vector<uint8_t> turned;
            BuildLook(type, rotation, turned);
            std::sort(turned.begin(), turned.end());
            EXPECT_EQ(base, turned) << type.name << " rotation " << (int)rotation;
        }
    }
}

TEST(BuildingLookTest, HasWallsRoofAndADoorOnTheFront) {
    const BuildingType& house = BUILDING_TYPES[BUILDING_FARMER_HOUSE];
    const int width = house.footprintWidth * TILE_SIZE;
    const int depth = house.footprintDepth * TILE_SIZE;
    // Where the door's left column lands for each rotation (front faces -z, +x, +z, -x)
    const glm::ivec2 door[4] = {
        { width / 2 - 1, 1 },
        { depth - 2, width / 2 - 1 },
        { width - 1 - (width / 2 - 1), depth - 2 },
        { 1, width - 1 - (width / 2 - 1) },
    };
    for (uint8_t rotation = 0; rotation < 4; rotation++) {
        std::vector<uint8_t> ids;
        BuildLook(house, rotation, ids);
        glm::ivec2 size = FootprintColumns(house, rotation);
        EXPECT_EQ(At(ids, size, door[rotation].x, 0, door[rotation].y), Block::WOOD) << "rotation " << (int)rotation;
        EXPECT_EQ(std::count(ids.begin(), ids.end(), house.wallBlock) > 0, true);
        // The ridge is the top layer and is roof
        EXPECT_EQ(At(ids, size, size.x / 2, BuildingHeight(house) - 1, size.y / 2), house.roofBlock);
        // Ground layer of the overhang ring stays empty (eaves only)
        EXPECT_EQ(At(ids, size, 0, 0, 0), Block::AIR);
    }
}

TEST(BuildingLookTest, UpgradedHouseFitsTheSameFootprintAndIsTaller) {
    const BuildingType& farmer = BUILDING_TYPES[BUILDING_FARMER_HOUSE];
    const BuildingType& worker = BUILDING_TYPES[BUILDING_WORKER_HOUSE];
    for (uint8_t rotation = 0; rotation < 4; rotation++) {
        EXPECT_EQ(FootprintTiles(farmer, rotation), FootprintTiles(worker, rotation));
    }
    EXPECT_GT(BuildingHeight(worker), BuildingHeight(farmer));
}

TEST(BuildingLookTest, MarketplaceIsAnOpenStall) {
    const BuildingType& market = BUILDING_TYPES[BUILDING_MARKETPLACE];
    std::vector<uint8_t> ids;
    BuildLook(market, 0, ids);
    glm::ivec2 size = FootprintColumns(market, 0);
    EXPECT_GT(std::count(ids.begin(), ids.end(), Block::AWNING), 0);
    // The middle of the front is open at counter height (no door, no wall)
    EXPECT_EQ(At(ids, size, size.x / 2, 0, 1), Block::AIR);
    EXPECT_EQ(At(ids, size, size.x / 2, 1, 1), Block::AIR);
}

TEST(BuildingLookTest, ConstructionRisesFromNothingToTheFinishedLook) {
    const BuildingType& type = BUILDING_TYPES[0];
    std::vector<uint8_t> look, out;
    BuildLook(type, 0, look);
    glm::ivec2 columns = FootprintColumns(type, 0);
    glm::ivec3 size(columns.x, BuildingHeight(type), columns.y);

    ConstructionLook(look, size, 0.0f, out);
    EXPECT_TRUE(std::all_of(out.begin(), out.end(), [](uint8_t id) { return id == Block::AIR; }));
    ConstructionLook(look, size, 1.0f, out);
    EXPECT_EQ(out, look);

    // Halfway: the ground layer is finished, the top is still air, and timber stands in between
    ConstructionLook(look, size, 0.5f, out);
    size_t layer = (size_t)size.x * size.z;
    EXPECT_TRUE(std::equal(out.begin(), out.begin() + layer, look.begin()));
    EXPECT_TRUE(std::all_of(out.end() - layer, out.end(), [](uint8_t id) { return id == Block::AIR; }));
    EXPECT_NE(std::find(out.begin(), out.end(), Block::TIMBER_LIGHT), out.end());
}
