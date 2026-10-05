#pragma once

#include <cstdint>

// Voxel block IDs, one byte per voxel. The shaders use the same values: colors in getAlbedo
// (shaders/render/shade.comp), WATER_ID / SAND_ID in shaders/include/scene.glsl.
namespace Block {
constexpr uint8_t AIR = 0;
constexpr uint8_t GRASS = 1;
constexpr uint8_t DIRT = 2;
constexpr uint8_t STONE = 3;
constexpr uint8_t SAND = 4;
constexpr uint8_t PLANT = 5;        // Hotbar block (also a tree palette color)
constexpr uint8_t CAVERN_GLOW = 30; // Bioluminescent cavern wall
constexpr uint8_t GRASS_TUFT = 34;  // Generated grass, animated by shaders/grass/grass_animate.comp
constexpr uint8_t WATER = 35;       // Ocean up to SEA_LEVEL; waves are applied while ray tracing
// The other IDs from 5 to 39 are tree model palette colors (see VoxModel).
} // namespace Block

// Grass tufts and water are drawn, but the player, raycasts and projectiles pass through them
inline bool IsSolidBlock(uint8_t id) {
    return id != Block::AIR && id != Block::GRASS_TUFT && id != Block::WATER;
}
