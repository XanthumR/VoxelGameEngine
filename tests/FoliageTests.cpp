#include "Simulation/TreeRegistry.h"
#include "TestWorld.h"
#include "World/Foliage.h"
#include "World/Islands.h"
#include "World/Sessions.h"
#include "World/WorldConstants.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

struct Root {
    glm::ivec2 column;
    int model;
};

std::vector<Root> RootsAround(TerrainGenerator& terrain, glm::ivec2 center, int radius) {
    std::vector<Root> roots;
    terrain.ForEachFoliageRoot(center - radius, center + radius, [&](glm::ivec2 root, int model) { roots.push_back({ root, model }); });
    return roots;
}

} // namespace

TEST(FoliageTest, EveryKindLoadsAndFitsTheReach) {
    const FoliageLibrary& foliage = TestWorld::Get().foliage;
    ASSERT_FALSE(foliage.IsEmpty());
    for (int model = 0; model < (int)FOLIAGE_KINDS.size() * 4; model++) {
        const VoxModel& voxels = foliage.Model(model);
        EXPECT_FALSE(voxels.IsEmpty()) << FOLIAGE_KINDS[FoliageKindOf(model)].file;
        EXPECT_LE(glm::max(-voxels.min.x, -voxels.min.z), foliage.Reach());
        EXPECT_LE(glm::max(voxels.max.x, voxels.max.z), foliage.Reach());
        EXPECT_LE(voxels.max.y, foliage.Height());
        EXPECT_GE(voxels.min.y, 0);
    }
}

TEST(FoliageTest, TheStartIslandHasForestsAndMeadows) {
    TestWorld& test = TestWorld::Get();
    // Blocks of 4 x 4 tiles wholly on land: some thick with trees, some open
    int forests = 0, meadows = 0, decor = 0;
    constexpr int BLOCK = 4 * TILE_SIZE;
    for (int bz = -6; bz < 6; bz++) {
        for (int bx = -6; bx < 6; bx++) {
            glm::ivec2 minColumn = test.spawnColumn + glm::ivec2(bx, bz) * BLOCK;
            bool land = true;
            for (int z = 0; z < 4; z++) {
                for (int x = 0; x < 4; x++) land = land && test.terrain.IsLandTile(ColumnToTile(minColumn.x) + x, ColumnToTile(minColumn.y) + z);
            }
            if (!land) continue;
            int trees = 0;
            test.terrain.ForEachFoliageRoot(minColumn, minColumn + BLOCK - 1, [&](glm::ivec2, int model) { IsTreeModel(model) ? trees++ : decor++; });
            forests += trees >= 15;
            meadows += trees <= 2;
        }
    }
    EXPECT_GT(forests, 0);
    EXPECT_GT(meadows, 0);
    EXPECT_GT(decor, 0);
}

TEST(FoliageTest, EachRootIsFoundAtItsColumn) {
    TestWorld& test = TestWorld::Get();
    std::vector<Root> roots = RootsAround(test.terrain, test.spawnColumn, 150);
    ASSERT_FALSE(roots.empty());
    for (const Root& root : roots) EXPECT_EQ(test.terrain.FoliageAt(root.column.x, root.column.y), root.model);
}

TEST(FoliageTest, EachSessionGrowsItsClimate) {
    TestWorld& test = TestWorld::Get();
    for (int session = 0; session < SESSION_COUNT; session++) {
        Climate own = SESSIONS[session].tropical ? Climate::Tropical : Climate::Temperate;
        glm::ivec2 center = IslandsOf(session).sites[0].center * TILE_SIZE;
        std::vector<Root> roots = RootsAround(test.terrain, center, 200);
        ASSERT_FALSE(roots.empty()) << session;
        for (const Root& root : roots) {
            Climate climate = FOLIAGE_KINDS[FoliageKindOf(root.model)].climate;
            EXPECT_TRUE(climate == own || climate == Climate::Both) << FOLIAGE_KINDS[FoliageKindOf(root.model)].file;
        }
    }
}

TEST(FoliageTest, LumberjacksSeeOnlyTreesButBuildingsClearEverything) {
    TestWorld& test = TestWorld::Get();
    TreeRegistry registry(test.terrain);
    glm::ivec2 minColumn = test.spawnColumn - 150, maxColumn = test.spawnColumn + 150;
    int trees = 0;
    glm::ivec2 rock(0);
    bool foundDecor = false;
    test.terrain.ForEachFoliageRoot(minColumn, maxColumn, [&](glm::ivec2 root, int model) {
        if (IsTreeModel(model)) trees++;
        else if (!foundDecor) {
            rock = root;
            foundDecor = true;
        }
    });
    EXPECT_EQ(registry.CountStanding(minColumn, maxColumn), trees);
    ASSERT_TRUE(foundDecor);
    EXPECT_TRUE(registry.AnyTreeOver(rock, rock));
    registry.FellOver(rock, rock, 0);
    EXPECT_TRUE(registry.IsFelled(rock));
    EXPECT_EQ(registry.CountStanding(minColumn, maxColumn), trees); // Not a tree: the lumberjacks' count is the same
}
