#include "Gameplay/SoundScape.h"
#include "Simulation/BuildingTypes.h"

#include <gtest/gtest.h>

namespace {

GameObjectId AddBuilding(GameObjectRegistry& objects, uint16_t type, glm::ivec2 column) {
    GameObjectId id = objects.Create();
    objects.Building(id).type = type;
    objects.Anchor(id).origin = glm::ivec3(column.x, 46, column.y);
    objects.Anchor(id).footprint = glm::ivec2(36);
    return id;
}

} // namespace

TEST(SoundScapeTest, EveryBuildingWithAModelHasItsSoundOrNone) {
    EXPECT_EQ(AmbienceOf(BUILDING_SAWMILL), Ambience::Saw);
    EXPECT_EQ(AmbienceOf(BUILDING_FURNACE), Ambience::Smithy);
    EXPECT_EQ(AmbienceOf(BUILDING_FARMER_HOUSE), Ambience::Town);
    EXPECT_EQ(AmbienceOf(BUILDING_HARBOR), Ambience::Harbor);
    EXPECT_EQ(AmbienceOf(BUILDING_WAREHOUSE), Ambience::Count);
}

TEST(SoundScapeTest, WorkingBuildingsNearTheCameraAreHeard) {
    GameObjectRegistry objects;
    GameObjectId sawmill = AddBuilding(objects, BUILDING_SAWMILL, glm::ivec2(0));
    glm::vec2 focus(18.0f); // Its middle

    // Not working: silent
    objects.Production(sawmill).status = ProducerStatus::NoRoad;
    EXPECT_EQ(AmbienceLevels(objects, focus, 200.0f)[(size_t)Ambience::Saw], 0.0f);

    objects.Production(sawmill).status = ProducerStatus::Working;
    std::array<float, AMBIENCE_COUNT> near = AmbienceLevels(objects, focus, 200.0f);
    EXPECT_GT(near[(size_t)Ambience::Saw], 0.1f);
    EXPECT_EQ(near[(size_t)Ambience::Smithy], 0.0f);
    EXPECT_GT(near[(size_t)Ambience::Sea], 0.0f); // Always there

    // Farther from it, or zoomed out: quieter; far off: silent
    EXPECT_LT(AmbienceLevels(objects, focus + glm::vec2(120.0f, 0.0f), 200.0f)[(size_t)Ambience::Saw], near[(size_t)Ambience::Saw]);
    EXPECT_LT(AmbienceLevels(objects, focus, 1200.0f)[(size_t)Ambience::Saw], near[(size_t)Ambience::Saw]);
    EXPECT_EQ(AmbienceLevels(objects, focus + glm::vec2(2000.0f, 0.0f), 200.0f)[(size_t)Ambience::Saw], 0.0f);
    // The wind rises with the camera
    EXPECT_GT(AmbienceLevels(objects, focus, 1200.0f)[(size_t)Ambience::Wind], near[(size_t)Ambience::Wind]);

    // More of them are louder, but never past full
    for (int i = 0; i < 30; i++) objects.Production(AddBuilding(objects, BUILDING_SAWMILL, glm::ivec2(0))).status = ProducerStatus::Working;
    float many = AmbienceLevels(objects, focus, 200.0f)[(size_t)Ambience::Saw];
    EXPECT_GT(many, near[(size_t)Ambience::Saw]);
    EXPECT_LE(many, 1.0f);
}
