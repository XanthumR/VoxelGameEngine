#include "TestWorld.h"
#include "World/TerrainGenerator.h"
#include "World/WorldConstants.h"

#include <gtest/gtest.h>

#include <cstdlib>

namespace {

using TileKind = TerrainGenerator::TileKind;
constexpr int LAND_TOP = SEA_LEVEL + ISLAND_HEIGHT;
constexpr int AREA_TILES = 30; // Tiles checked on each side of the spawn tile

glm::ivec2 SpawnTile() {
    glm::ivec2 spawn = TestWorld::Get().spawnColumn;
    return glm::ivec2(ColumnToTile(spawn.x), ColumnToTile(spawn.y));
}

template <typename Function>
void ForEachTileAroundSpawn(Function function) {
    glm::ivec2 center = SpawnTile();
    for (int tz = center.y - AREA_TILES; tz <= center.y + AREA_TILES; tz++) {
        for (int tx = center.x - AREA_TILES; tx <= center.x + AREA_TILES; tx++) function(tx, tz);
    }
}

bool NearCliff(TerrainGenerator& terrain, int wx, int wz) {
    int tx = ColumnToTile(wx), tz = ColumnToTile(wz);
    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (terrain.TileKindAt(tx + dx, tz + dz) == TileKind::Cliff) return true;
        }
    }
    return false;
}

} // namespace

TEST(TerrainTest, IslandsAreMadeOfWholeTiles) {
    TerrainGenerator& terrain = TestWorld::Get().terrain;
    int landTiles = 0;
    ForEachTileAroundSpawn([&](int tx, int tz) {
        bool land = terrain.TileKindAt(tx, tz) == TileKind::Land;
        landTiles += land ? 1 : 0;
        for (int z = 0; z < TILE_SIZE; z++) {
            for (int x = 0; x < TILE_SIZE; x++) {
                int height = terrain.TerrainHeightAt(tx * TILE_SIZE + x, tz * TILE_SIZE + z);
                if (land) ASSERT_EQ(height, LAND_TOP) << "tile " << tx << "," << tz; // Flat grass, all of it
                else ASSERT_LT(height, LAND_TOP) << "tile " << tx << "," << tz;
            }
        }
    });
    EXPECT_GT(landTiles, 100);
}

TEST(TerrainTest, CoastTilesTouchLandAndComeInBothKinds) {
    TerrainGenerator& terrain = TestWorld::Get().terrain;
    int beaches = 0, cliffs = 0;
    ForEachTileAroundSpawn([&](int tx, int tz) {
        TileKind kind = terrain.TileKindAt(tx, tz);
        bool touchesLand = false;
        for (int dz = -1; dz <= 1; dz++) {
            for (int dx = -1; dx <= 1; dx++) touchesLand |= (dx != 0 || dz != 0) && terrain.IsLandTile(tx + dx, tz + dz);
        }
        if (kind == TileKind::Beach || kind == TileKind::Cliff) EXPECT_TRUE(touchesLand);
        if (kind == TileKind::Sea) EXPECT_FALSE(touchesLand);
        beaches += kind == TileKind::Beach ? 1 : 0;
        cliffs += kind == TileKind::Cliff ? 1 : 0;
    });
    EXPECT_GT(beaches, 0);
    EXPECT_GT(cliffs, 0);
}

TEST(TerrainTest, NoLoneLandTiles) {
    TerrainGenerator& terrain = TestWorld::Get().terrain;
    ForEachTileAroundSpawn([&](int tx, int tz) {
        if (!terrain.IsLandTile(tx, tz)) return;
        int neighbours = 0;
        for (glm::ivec2 step : { glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1) }) {
            neighbours += terrain.IsLandTile(tx + step.x, tz + step.y) ? 1 : 0;
        }
        EXPECT_GE(neighbours, 1) << "tile " << tx << "," << tz;
    });
}

TEST(TerrainTest, BeachSlopesFromTheGrassIntoTheSea) {
    TerrainGenerator& terrain = TestWorld::Get().terrain;
    int checked = 0;
    ForEachTileAroundSpawn([&](int tx, int tz) {
        if (terrain.TileKindAt(tx, tz) != TileKind::Beach || !terrain.IsLandTile(tx, tz - 1)) return;
        bool landElsewhere = terrain.IsLandTile(tx - 1, tz) || terrain.IsLandTile(tx + 1, tz);
        for (int dx = -1; dx <= 1; dx++) landElsewhere |= terrain.IsLandTile(tx + dx, tz + 1);
        if (landElsewhere) return;
        // Land only to the north (straight coast): going south across the tile the sand goes down, from above the
        // sea to under it (heights are the first air voxel, so SEA_LEVEL is under water)
        int wx = tx * TILE_SIZE + TILE_SIZE / 2;
        int previous = LAND_TOP;
        for (int z = 0; z < TILE_SIZE; z++) {
            int height = terrain.TerrainHeightAt(wx, tz * TILE_SIZE + z);
            EXPECT_LE(height, previous);
            previous = height;
        }
        EXPECT_GT(terrain.TerrainHeightAt(wx, tz * TILE_SIZE), SEA_LEVEL);
        EXPECT_LE(terrain.TerrainHeightAt(wx, tz * TILE_SIZE + TILE_SIZE - 1), SEA_LEVEL);
        checked++;
    });
    EXPECT_GT(checked, 0);
}

TEST(TerrainTest, SeaFloorHasNoStepsAwayFromCliffs) {
    // Neighbouring underwater and beach columns differ by at most 2 voxels (no pits or walls under
    // the water); the sea is deep at once only in front of cliffs
    TerrainGenerator& terrain = TestWorld::Get().terrain;
    glm::ivec2 center = SpawnTile() * TILE_SIZE;
    const int range = AREA_TILES * TILE_SIZE;
    for (int wz = center.y - range; wz < center.y + range; wz += 3) {
        for (int wx = center.x - range; wx < center.x + range; wx++) {
            if (terrain.IsLandTile(ColumnToTile(wx), ColumnToTile(wz)) || terrain.IsLandTile(ColumnToTile(wx + 1), ColumnToTile(wz))) continue;
            if (NearCliff(terrain, wx, wz) || NearCliff(terrain, wx + 1, wz)) continue;
            int step = std::abs(terrain.TerrainHeightAt(wx, wz) - terrain.TerrainHeightAt(wx + 1, wz));
            ASSERT_LE(step, 2) << "at " << wx << "," << wz;
        }
    }
}

TEST(TerrainTest, CliffsDropIntoDeepWater) {
    TerrainGenerator& terrain = TestWorld::Get().terrain;
    int checked = 0;
    ForEachTileAroundSpawn([&](int tx, int tz) {
        if (terrain.TileKindAt(tx, tz) != TileKind::Cliff) return;
        EXPECT_LE(terrain.TerrainHeightAt(tx * TILE_SIZE + TILE_SIZE / 2, tz * TILE_SIZE + TILE_SIZE / 2), SEA_LEVEL - 6);
        checked++;
    });
    EXPECT_GT(checked, 0);
}
