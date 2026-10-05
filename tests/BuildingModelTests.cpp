#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingModels.h"
#include "World/BuildingModel.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

// ASSET_DIRECTORY is defined by the test project (the repository's assets folder)
namespace {

const std::string BUILDINGS = std::string(ASSET_DIRECTORY) + "/buildings";

BuildingModelLibrary& Library() {
    static BuildingModelLibrary library;
    static bool loaded = false;
    if (!loaded) {
        library.LoadAll(BUILDINGS);
        loaded = true;
    }
    return library;
}

} // namespace

TEST(BuildingModelTest, EveryTypeHasAModelThatFits) {
    for (uint16_t type = 0; type < BUILDING_TYPES.size(); type++) {
        EXPECT_GE(Library().VariantCount(type), 1) << BUILDING_TYPES[type].name;
    }
}

TEST(BuildingModelTest, LoadsSizeAndBlocks) {
    BuildingModel model;
    ASSERT_TRUE(model.Load(BUILDINGS + "/farmer_house_1.vox"));
    EXPECT_EQ(model.width, 36);
    EXPECT_EQ(model.depth, 36);
    EXPECT_EQ(model.height, 30);
    EXPECT_TRUE(BuildingModelLibrary::Fits(model, BUILDING_TYPES[BUILDING_FARMER_HOUSE]));
    // Every voxel is a known block: air or a building material, nothing outside the palette rules
    for (uint8_t id : model.ids) {
        EXPECT_TRUE(id == Block::AIR || (id >= Block::MODEL_MATERIALS_FIRST && id <= Block::MODEL_MATERIALS_LAST)) << (int)id;
    }
}

TEST(BuildingModelTest, MissingFileFails) {
    BuildingModel model;
    EXPECT_FALSE(model.Load(BUILDINGS + "/no_such_building_1.vox"));
    EXPECT_TRUE(model.IsEmpty());
}

TEST(BuildingModelTest, WrongSizeDoesNotFit) {
    BuildingModel model;
    ASSERT_TRUE(model.Load(BUILDINGS + "/farmer_house_1.vox"));
    EXPECT_FALSE(BuildingModelLibrary::Fits(model, BUILDING_TYPES[BUILDING_WAREHOUSE]));
}

TEST(BuildingModelTest, RotationKeepsTheSameVoxels) {
    for (uint16_t type = 0; type < BUILDING_TYPES.size(); type++) {
        std::vector<uint8_t> base;
        Library().BuildVoxels(type, 0, 0, base);
        glm::ivec2 size = FootprintColumns(BUILDING_TYPES[type], 0);
        EXPECT_EQ(base.size(), (size_t)size.x * size.y * BuildingVolumeHeight(BUILDING_TYPES[type]));
        std::sort(base.begin(), base.end());
        for (uint8_t rotation = 1; rotation < 4; rotation++) {
            std::vector<uint8_t> turned;
            Library().BuildVoxels(type, 0, rotation, turned);
            std::sort(turned.begin(), turned.end());
            EXPECT_EQ(base, turned) << BUILDING_TYPES[type].name << " rotation " << (int)rotation;
        }
    }
}

TEST(BuildingModelTest, DoorFacesTheFrontInEveryRotation) {
    // The farmer house's door (block DOOR_WOOD) is in its front wall: after rotating, the door
    // voxels must lie in the half of the footprint the front faces (0 = -z, 1 = +x, 2 = +z, 3 = -x)
    const BuildingType& house = BUILDING_TYPES[BUILDING_FARMER_HOUSE];
    for (uint8_t rotation = 0; rotation < 4; rotation++) {
        std::vector<uint8_t> ids;
        Library().BuildVoxels(BUILDING_FARMER_HOUSE, 0, rotation, ids);
        glm::ivec2 size = FootprintColumns(house, rotation);
        int doors = 0;
        for (size_t i = 0; i < ids.size(); i++) {
            if (ids[i] != Block::DOOR_WOOD) continue;
            int x = (int)(i % size.x), z = (int)((i / size.x) % size.y);
            bool front = rotation == 0 ? z < size.y / 2 : rotation == 1 ? x >= size.x / 2 : rotation == 2 ? z >= size.y / 2 : x < size.x / 2;
            EXPECT_TRUE(front) << "rotation " << (int)rotation;
            doors++;
        }
        EXPECT_GT(doors, 0);
    }
}

TEST(BuildingModelTest, VariantsSpreadOverNeighbours) {
    std::set<int> seen;
    for (uint32_t id = 1; id <= 20; id++) seen.insert(Library().PickVariant(BUILDING_FARMER_HOUSE, id));
    EXPECT_EQ((int)seen.size(), Library().VariantCount(BUILDING_FARMER_HOUSE));
}
