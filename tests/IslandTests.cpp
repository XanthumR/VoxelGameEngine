#include "World/Islands.h"
#include "World/TerrainGenerator.h"
#include "World/Foliage.h"

#include <gtest/gtest.h>

#include <array>

// Every island the session plans is placed, has land tiles within its size's range, and is apart
// from the others; the player starts on the Old World's first island, a Large one
TEST(IslandTest, EverySessionHasItsMixOfSizes) {
    const std::array<std::array<int, 4>, SESSION_COUNT> expected = { { { 5, 4, 3, 1 }, { 4, 3, 2, 0 } } };
    for (int session = 0; session < SESSION_COUNT; session++) {
        const SessionIslands& islands = IslandsOf(session);
        std::array<int, 4> counts = {};
        std::vector<int> tiles(islands.sites.size(), 0);
        for (uint8_t site : islands.map) {
            if (site) tiles[site - 1]++;
        }
        for (size_t i = 0; i < islands.sites.size(); i++) {
            const IslandSite& site = islands.sites[i];
            const IslandSizeClass& range = ISLAND_SIZES[(int)site.size];
            counts[(int)site.size]++;
            EXPECT_EQ(tiles[i], site.tiles);
            EXPECT_GE(site.tiles, range.minTiles) << SESSIONS[session].name << " island " << i;
            EXPECT_LE(site.tiles, range.maxTiles) << SESSIONS[session].name << " island " << i;
            EXPECT_TRUE(IsIslandTile(site.center));
        }
        EXPECT_EQ(counts, expected[session]) << SESSIONS[session].name;
    }
    const SessionIslands& old = IslandsOf(SESSION_OLD_WORLD);
    ASSERT_GE(old.start, 0);
    EXPECT_EQ(old.sites[old.start].size, IslandSize::Large);
}

TEST(IslandTest, IslandsKeepTheirDistance) {
    for (int session = 0; session < SESSION_COUNT; session++) {
        const SessionIslands& islands = IslandsOf(session);
        for (int z = 0; z < SESSION_TILES; z++) {
            for (int x = 0; x < SESSION_TILES; x++) {
                uint8_t site = islands.map[z * SESSION_TILES + x];
                if (!site) continue;
                ASSERT_TRUE(x >= EDGE_SEA_TILES && z >= EDGE_SEA_TILES && x < SESSION_TILES - EDGE_SEA_TILES && z < SESSION_TILES - EDGE_SEA_TILES);
                // Only the same island within the gap, checked along the axes
                for (int d = 1; d <= ISLAND_GAP; d++) {
                    for (glm::ivec2 other : { glm::ivec2(x + d, z), glm::ivec2(x, z + d) }) {
                        if (other.x >= SESSION_TILES || other.y >= SESSION_TILES) continue;
                        uint8_t near = islands.map[other.y * SESSION_TILES + other.x];
                        ASSERT_TRUE(near == 0 || near == site) << SESSIONS[session].name << " at " << x << ", " << z;
                    }
                }
            }
        }
    }
}

TEST(IslandTest, TheSpawnIsOnTheStartIsland) {
    FoliageLibrary noFoliage;
    TerrainGenerator terrain(noFoliage);
    glm::ivec2 spawn = terrain.FindSpawnColumn(SpawnSearchColumn());
    glm::ivec2 local = glm::ivec2(ColumnToTile(spawn.x), ColumnToTile(spawn.y)) - SESSIONS[SESSION_OLD_WORLD].originTile;
    const SessionIslands& old = IslandsOf(SESSION_OLD_WORLD);
    EXPECT_EQ(old.map[local.y * SESSION_TILES + local.x], old.start + 1);
}
