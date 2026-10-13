#pragma once

#include "World/TerrainGenerator.h"
#include "World/Foliage.h"
#include "World/VoxelWorld.h"

#include <glm/glm.hpp>

#include <cstdint>

struct PlacementContext;

// The real generated world around the spawn island, built once and shared by the tests that need
// voxels. Only chunk layer 1 (y 32-63) is generated: it holds the sea surface, the island ground
// and everything a building stands on. The voxels have no foliage (the placement tests expect bare
// ground), but terrain knows where it stands. More of the world can be generated on demand with
// LoadAround.
struct TestWorld {
    static constexpr int RADIUS_CHUNKS = 8; // Generated around the spawn chunk up front

    FoliageLibrary foliage = LoadFoliage();
    TerrainGenerator terrain{ foliage };
    FoliageLibrary noFoliage;
    TerrainGenerator bareTerrain{ noFoliage }; // Generates the chunks
    VoxelWorld world;
    glm::ivec2 spawnColumn = glm::ivec2(0);

    static TestWorld& Get();
    static FoliageLibrary LoadFoliage(); // The game's foliage models from ASSET_DIRECTORY

    // Generates layer 1 of the chunks within radiusChunks of a column (already loaded ones are kept)
    void LoadAround(glm::ivec2 column, int radiusChunks);

    // The coastal spot nearest the spawn (in rings of tiles) where a building with dock rows may
    // stand, for that context; false when there is none. Spots whose land tiles are clearly too
    // many or too few are skipped before the full check, which keeps the search quick.
    bool FindCoastSpot(const PlacementContext& context, uint16_t type, glm::ivec2& minTile, uint8_t& rotation);

private:
    TestWorld();
};
