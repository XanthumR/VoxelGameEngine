#pragma once

// World layout and scale, shared by every system.
constexpr int CHUNK_SIZE = 32;                          // Chunks are CHUNK_SIZE^3 voxels
constexpr int CHUNK_LAYERS = 4;                         // Vertical chunk count
constexpr int WORLD_HEIGHT = CHUNK_SIZE * CHUNK_LAYERS; // 128 voxels
constexpr float VOXELS_PER_UNIT = 1280.0f;              // World space -> voxel space scale

// Ocean surface. 42 keeps the wave layers (40-44) in the same 8^3 brick and 32^3 chunk as the
// water, so the ray marcher never skips past a raised wave (see seaWave in shaders/include/water.glsl).
constexpr int SEA_LEVEL = 42;
constexpr int ISLAND_HEIGHT = 4; // Islands are flat, this many voxels above the sea
