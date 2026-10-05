#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

// One animated grass tuft, 32 bytes to match the std430 layout in anim.comp
struct AnimatedPlant {
    glm::ivec4 posAndPose; // xyz = root (first air voxel above the ground), w = current pose code
    glm::vec4 params;      // x = wind phase offset, y = blade variant (0..3), z = 1 if retired (return to rest), w = unused
};

inline std::vector<AnimatedPlant> activePlants;

// --- Grass Tuft Placement (shared by the terrain generator and the animation) ---

// Grass tufts are switched off for now (islands are flat building ground). Set to true to bring
// back the generated tufts and their wind animation.
constexpr bool GRASS_TUFTS_ENABLED = false;

constexpr int GRASS_CELL = 4;       // One candidate tuft per 4x4 cell of columns
constexpr int GRASS_BLADES = 3;     // Blades per tuft
constexpr int GRASS_MAX_HEIGHT = 4; // Tallest blade

// [variant][blade] = { x offset, z offset, height }. Must match BLADES in anim.comp.
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
inline uint32_t grassCellHash(int gx, int gz) {
    uint32_t h = (uint32_t)gx * 0x8da6b343u ^ (uint32_t)gz * 0xd8163841u ^ 0x9e3779b9u;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}
