#pragma once

#include "World/TerrainGenerator.h"
#include "World/VoxModel.h"
#include "World/VoxelWorld.h"

#include <glm/glm.hpp>

// The real generated world around the spawn island, built once and shared by the tests that need
// voxels. Only chunk layer 1 (y 32-63) is generated: it holds the sea surface, the island ground
// and everything a building occupies. No trees (the tree model is not loaded).
struct TestWorld {
    static constexpr int RADIUS_CHUNKS = 6; // Around the spawn chunk

    VoxModel trees;
    TerrainGenerator terrain{ trees };
    VoxelWorld world;
    glm::ivec2 spawnColumn = glm::ivec2(0);

    static TestWorld& Get();

private:
    TestWorld();
};
