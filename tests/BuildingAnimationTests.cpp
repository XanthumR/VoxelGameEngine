#include "Gameplay/BuildingAnimations.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Placement.h"

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include <string>
#include <vector>

// ASSET_DIRECTORY is defined by the test project (the repository's assets folder)
namespace {

// The model's middle in the world: VoxelRenderer turns a model about its bottom middle
glm::vec3 Middle(const VoxelObject& object, float pivotHeight) {
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), object.yaw, glm::vec3(0, 1, 0));
    rotation = glm::rotate(rotation, -object.pitch, glm::vec3(1, 0, 0));
    rotation = glm::rotate(rotation, object.roll, glm::vec3(0, 0, 1));
    return object.position + glm::vec3(rotation * glm::vec4(0.0f, pivotHeight, 0.0f, 0.0f));
}

} // namespace

TEST(BuildingAnimationTest, EveryPartLoadsOntoABuilding) {
    BuildingAnimations animations;
    std::vector<VoxelObjectModel> models;
    ASSERT_TRUE(animations.Load(std::string(ASSET_DIRECTORY) + "/buildings/parts", models));
    EXPECT_GT(animations.Parts().size(), 30u);
    for (const BuildingAnimations::Part& part : animations.Parts()) {
        ASSERT_LT(part.model, (int)models.size());
        const VoxelObjectModel& model = models[part.model];
        EXPECT_EQ(model.ids.size(), (size_t)model.size.x * model.size.y * model.size.z);
    }
}

// A spinning part (the windmill's sails) turns about its pivot: its middle stays put, whatever the
// building's rotation
TEST(BuildingAnimationTest, SpinningPartKeepsItsPivot) {
    BuildingAnimations::Part sails;
    sails.buildingType = 17; // Flour Mill
    sails.pivot = glm::vec3(18.5f, 6.5f, 44.5f);
    sails.pivotHeight = 20.5f;
    sails.motion = BuildingAnimations::Motion::Spin;
    sails.axis = BuildingAnimations::Axis::Forward;
    sails.a = 0.8f;
    VoxelAnchorComponent anchor;
    anchor.origin = glm::ivec3(120, BUILD_GROUND_Y, 240);
    for (uint8_t rotation = 0; rotation < 4; rotation++) {
        BuildingComponent building;
        building.type = 17;
        building.rotation = rotation;
        glm::vec3 first = Middle(BuildingAnimations::Place(sails, building, anchor, 0.0f, 0.0f), sails.pivotHeight);
        for (float time : { 0.7f, 2.0f, 9.3f }) {
            VoxelObject object = BuildingAnimations::Place(sails, building, anchor, time, 0.0f);
            glm::vec3 middle = Middle(object, sails.pivotHeight);
            EXPECT_NEAR(glm::distance(middle, first), 0.0f, 1e-3f);
            EXPECT_NE(object.roll, 0.0f);
        }
        EXPECT_NEAR(first.y, BUILD_GROUND_Y + 44.5f, 1e-3f);
    }
    BuildingComponent building;
    building.type = 17;
    glm::vec3 unrotated = Middle(BuildingAnimations::Place(sails, building, anchor, 0.0f, 0.0f), sails.pivotHeight);
    EXPECT_NEAR(unrotated.x, 120 + 18.5f, 1e-3f); // Rotation 0: u along x, v along z
    EXPECT_NEAR(unrotated.z, 240 + 6.5f, 1e-3f);
}

TEST(BuildingAnimationTest, AnimalsStayInTheirPen) {
    BuildingAnimations::Part sheep;
    sheep.buildingType = BUILDING_SHEEPFOLD;
    sheep.pivot = glm::vec3(16.5f, 13.5f, 0.0f);
    sheep.motion = BuildingAnimations::Motion::Wander;
    sheep.a = 8.0f;
    sheep.b = 8.0f;
    BuildingComponent building;
    building.type = BUILDING_SHEEPFOLD;
    VoxelAnchorComponent anchor;
    anchor.origin = glm::ivec3(0, BUILD_GROUND_Y, 0);
    for (float time = 0.0f; time < 300.0f; time += 0.37f) {
        VoxelObject object = BuildingAnimations::Place(sheep, building, anchor, time, 0.0f);
        EXPECT_GE(object.position.x, 16.5f - 8.0f - 1e-3f);
        EXPECT_LE(object.position.x, 16.5f + 8.0f + 1e-3f);
        EXPECT_GE(object.position.z, 13.5f - 8.0f - 1e-3f);
        EXPECT_LE(object.position.z, 13.5f + 8.0f + 1e-3f);
        EXPECT_GE(object.position.y, (float)BUILD_GROUND_Y);
    }
}
