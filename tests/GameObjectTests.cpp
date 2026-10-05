#include "Simulation/GameObjects.h"
#include "Simulation/OccupancyGrid.h"

#include <gtest/gtest.h>

#include <memory>

TEST(GameObjectTest, CreateGivesDistinctLiveIds) {
    GameObjectRegistry objects;
    GameObjectId a = objects.Create();
    GameObjectId b = objects.Create();
    EXPECT_NE(a, INVALID_GAME_OBJECT);
    EXPECT_NE(b, INVALID_GAME_OBJECT);
    EXPECT_NE(a, b);
    EXPECT_TRUE(objects.IsAlive(a));
    EXPECT_TRUE(objects.IsAlive(b));
    EXPECT_EQ(objects.AliveCount(), 2u);
}

TEST(GameObjectTest, DestroyedSlotIsReusedButOldIdStaysDead) {
    GameObjectRegistry objects;
    GameObjectId a = objects.Create();
    objects.Create();
    EXPECT_TRUE(objects.Destroy(a));
    EXPECT_FALSE(objects.Destroy(a)); // Twice is a no-op
    EXPECT_FALSE(objects.IsAlive(a));

    GameObjectId c = objects.Create();
    EXPECT_EQ(c & 0xFFFF, a & 0xFFFF); // Same slot
    EXPECT_NE(c, a);                   // New generation
    EXPECT_FALSE(objects.IsAlive(a));
    EXPECT_TRUE(objects.IsAlive(c));
    EXPECT_EQ(objects.AliveCount(), 2u);
}

TEST(GameObjectTest, ComponentsAreResetOnCreate) {
    GameObjectRegistry objects;
    GameObjectId a = objects.Create();
    objects.Building(a).type = 7;
    objects.Anchor(a).origin = glm::ivec3(1, 2, 3);
    objects.Destroy(a);
    GameObjectId b = objects.Create();
    EXPECT_EQ(objects.Building(b).type, 0);
    EXPECT_EQ(objects.Anchor(b).origin, glm::ivec3(0));
}

TEST(GameObjectTest, FullRegistryReturnsInvalid) {
    auto objects = std::make_unique<GameObjectRegistry>();
    for (uint32_t i = 0; i < GameObjectRegistry::MAX_OBJECTS; i++) ASSERT_NE(objects->Create(), INVALID_GAME_OBJECT);
    EXPECT_EQ(objects->Create(), INVALID_GAME_OBJECT);
    EXPECT_EQ(objects->AliveCount(), GameObjectRegistry::MAX_OBJECTS);
}

TEST(GameObjectTest, IterationVisitsLiveObjectsOnly) {
    GameObjectRegistry objects;
    GameObjectId a = objects.Create();
    GameObjectId b = objects.Create();
    GameObjectId c = objects.Create();
    objects.Destroy(b);
    int seen = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        EXPECT_TRUE(id == a || id == c);
        seen++;
    }
    EXPECT_EQ(seen, 2);
}

TEST(OccupancyTest, OverlapIsRejected) {
    OccupancyGrid grid;
    EXPECT_TRUE(grid.Occupy({ 0, 0 }, { 4, 4 }, 1));
    EXPECT_FALSE(grid.Occupy({ 3, 3 }, { 3, 3 }, 2));
    EXPECT_EQ(grid.At({ 3, 3 }), 1u);
    EXPECT_EQ(grid.At({ 4, 4 }), INVALID_GAME_OBJECT); // The failed claim wrote nothing
    EXPECT_TRUE(grid.IsFree({ 4, 0 }, { 3, 3 }));
    EXPECT_FALSE(grid.Occupy({ 10, 10 }, { 1, 1 }, INVALID_GAME_OBJECT));
}

TEST(OccupancyTest, NegativeTilesWork) {
    OccupancyGrid grid;
    EXPECT_TRUE(grid.Occupy({ -3, -3 }, { 3, 3 }, 5));
    EXPECT_EQ(grid.At({ -1, -1 }), 5u);
    EXPECT_EQ(grid.At({ 0, 0 }), INVALID_GAME_OBJECT);
}

TEST(OccupancyTest, ReleaseOnlyFreesTheOwnersTiles) {
    OccupancyGrid grid;
    grid.Occupy({ 0, 0 }, { 2, 2 }, 1);
    grid.Occupy({ 2, 0 }, { 2, 2 }, 2);
    grid.Release({ 0, 0 }, { 4, 2 }, 1); // Covers both; only object 1 is freed
    EXPECT_EQ(grid.At({ 0, 0 }), INVALID_GAME_OBJECT);
    EXPECT_EQ(grid.At({ 2, 0 }), 2u);
    EXPECT_EQ(grid.OccupiedTiles(), 4u);
}
