#pragma once

#include <glm/glm.hpp>

#include <cstdint>

// Grass tuft placement, shared by the terrain generator (bakes tufts into chunks in their rest
// pose) and GrassAnimator (bends the ones near the player).

// Grass tufts are switched off for now (islands are flat building ground). Set to true to bring
// back the generated tufts and their wind animation.
constexpr bool GRASS_TUFTS_ENABLED = false;

constexpr int GRASS_CELL = 4;       // One candidate tuft per 4x4 cell of columns
constexpr int GRASS_BLADES = 3;     // Blades per tuft
constexpr int GRASS_MAX_HEIGHT = 4; // Tallest blade

// [variant][blade] = { x offset, z offset, height }. Must match BLADES in grass_animate.comp.
inline const int GRASS_BLADE_SHAPES[4][GRASS_BLADES][3] = {
    { { 0, 0, 4 }, { 1, 1, 3 }, { -1, 1, 2 } },
    { { 0, 0, 3 }, { -1, 0, 4 }, { 1, -1, 2 } },
    { { 0, 0, 4 }, { 1, 0, 2 }, { 0, -1, 3 } },
    { { 0, 0, 3 }, { 0, 1, 3 }, { -1, -1, 2 } },
};

struct GrassTuft {
    glm::ivec3 root; // First air voxel above the grass block
    int variant;
    float phase;
};

// Deterministic per-cell hash (integer mixing), so placement needs no stored state
inline uint32_t GrassCellHash(int gx, int gz) {
    uint32_t h = (uint32_t)gx * 0x8da6b343u ^ (uint32_t)gz * 0xd8163841u ^ 0x9e3779b9u;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}
