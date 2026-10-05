#include "Simulation/IslandRegistry.h"
#include "TestWorld.h"
#include "World/WorldConstants.h"

#include <gtest/gtest.h>

#include <map>
#include <vector>

TEST(IslandRegistryTest, SpawnIsOnAnIsland) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    IslandId home = islands.IslandIdAt(test.spawnColumn.x, test.spawnColumn.y);
    ASSERT_NE(home, NO_ISLAND);

    const IslandInfo* info = islands.Info(home);
    ASSERT_NE(info, nullptr);
    EXPECT_GT(info->cellCount, 0);
    EXPECT_FALSE(info->truncated);
    EXPECT_GE(test.spawnColumn.x, info->minColumn.x);
    EXPECT_LE(test.spawnColumn.x, info->maxColumn.x);
    EXPECT_GE(test.spawnColumn.y, info->minColumn.y);
    EXPECT_LE(test.spawnColumn.y, info->maxColumn.y);
    EXPECT_EQ(islands.Info(NO_ISLAND), nullptr);
}

TEST(IslandRegistryTest, IdsAreStableAcrossQueries) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    IslandId home = islands.IslandIdAt(test.spawnColumn.x, test.spawnColumn.y);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(islands.IslandIdAt(test.spawnColumn.x, test.spawnColumn.y), home);
        EXPECT_EQ(islands.IslandIdAt(test.spawnColumn.x + 5, test.spawnColumn.y - 3), home);
    }
    EXPECT_EQ(islands.IslandCount(), 1u); // Querying the same island never adds another
}

TEST(IslandRegistryTest, WaterHasNoIsland) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    int checked = 0;
    for (int z = test.spawnColumn.y - 600; z < test.spawnColumn.y + 600; z += 17) {
        for (int x = test.spawnColumn.x - 600; x < test.spawnColumn.x + 600; x += 17) {
            if (test.terrain.TerrainHeightAt(x, z) > SEA_LEVEL) continue;
            EXPECT_EQ(islands.IslandIdAt(x, z), NO_ISLAND);
            checked++;
        }
    }
    EXPECT_GT(checked, 100);
}

// Islands separated by water get different IDs, and which columns share an island does not
// depend on the order the islands were discovered in
TEST(IslandRegistryTest, IslandsAreTheSameWhateverTheDiscoveryOrder) {
    TestWorld& test = TestWorld::Get();
    std::vector<glm::ivec2> landColumns;
    for (int z = test.spawnColumn.y - 1500; z < test.spawnColumn.y + 1500; z += 41) {
        for (int x = test.spawnColumn.x - 1500; x < test.spawnColumn.x + 1500; x += 41) {
            if (test.terrain.TerrainHeightAt(x, z) >= SEA_LEVEL + ISLAND_HEIGHT) landColumns.push_back({ x, z });
        }
    }
    ASSERT_GT(landColumns.size(), 20u);

    IslandRegistry forward(test.terrain), backward(test.terrain);
    std::vector<IslandId> forwardIds(landColumns.size()), backwardIds(landColumns.size());
    for (size_t i = 0; i < landColumns.size(); i++) forwardIds[i] = forward.IslandIdAt(landColumns[i].x, landColumns[i].y);
    for (size_t i = landColumns.size(); i-- > 0;) backwardIds[i] = backward.IslandIdAt(landColumns[i].x, landColumns[i].y);

    EXPECT_GT(forward.IslandCount(), 1u);
    EXPECT_EQ(forward.IslandCount(), backward.IslandCount());

    // The two ID assignments must be a one-to-one renaming of each other
    std::map<IslandId, IslandId> forwardToBackward, backwardToForward;
    for (size_t i = 0; i < landColumns.size(); i++) {
        auto [a, insertedA] = forwardToBackward.emplace(forwardIds[i], backwardIds[i]);
        auto [b, insertedB] = backwardToForward.emplace(backwardIds[i], forwardIds[i]);
        EXPECT_EQ(a->second, backwardIds[i]) << "column " << landColumns[i].x << "," << landColumns[i].y;
        EXPECT_EQ(b->second, forwardIds[i]) << "column " << landColumns[i].x << "," << landColumns[i].y;
    }
}
