#pragma once

#include "World/TerrainGenerator.h"
#include "World/VoxModel.h"
#include "World/VoxelWorld.h"

#include <glm/glm.hpp>

// The real generated world around the spawn island, built once and shared by the tests that need
// voxels. Only chunk layer 1 (y 32-63) is generated: it holds the sea surface, the island ground
// and everything a building stands on. No trees (the tree model is not loaded). More of the world
// can be generated on demand with LoadAround.
struct TestWorld {
    static constexpr int RADIUS_CHUNKS = 8; // Generated around the spawn chunk up front

    VoxModel trees;
    TerrainGenerator terrain{ trees };
    VoxelWorld world;
    glm::ivec2 spawnColumn = glm::ivec2(0);

    static TestWorld& Get();

    // Generates layer 1 of the chunks within radiusChunks of a column (already loaded ones are kept)
    void LoadAround(glm::ivec2 column, int radiusChunks);

private:
    TestWorld();
};
