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
// Building materials (the look of buildings; see src/Simulation/BuildingLook.cpp)
constexpr uint8_t WOOD = 40;        // Posts, door
constexpr uint8_t PLANK = 41;       // Timber walls
constexpr uint8_t ROOF = 42;        // Roof tiles
constexpr uint8_t STONE_WALL = 43;  // Masonry walls
// Roads replace the grass layer (see RoadTool): dirt, with a stone kerb along open sides
constexpr uint8_t ROAD_DIRT = 44;
constexpr uint8_t ROAD_EDGE = 45;
constexpr uint8_t AWNING = 46;     // Striped market awning (stripes drawn by the shader)
// Figures (people, smoke, boats): drawn straight into the GPU chunk data each frame
// (FigureRenderer), never in the CPU world
constexpr uint8_t PERSON_SKIN = 47;
constexpr uint8_t PERSON_TROUSERS = 48;
constexpr uint8_t PERSON_SHIRT_FARMER = 49;
constexpr uint8_t PERSON_SHIRT_WORKER = 50;
constexpr uint8_t PERSON_SKIN_DARK = 51;
constexpr uint8_t PERSON_TROUSERS_BLUE = 52;
constexpr uint8_t PERSON_SHIRT_FARMER_GREEN = 53;
constexpr uint8_t PERSON_SHIRT_WORKER_GREY = 54;
constexpr uint8_t PERSON_STRAW_HAT = 55;
constexpr uint8_t PERSON_CAP = 56;
constexpr uint8_t PERSON_SHOES = 57;
constexpr uint8_t PERSON_FIRST = PERSON_SKIN, PERSON_LAST = PERSON_SHOES;
constexpr uint8_t SMOKE_LIGHT = 58; // Smoke puffs, and sails
constexpr uint8_t BOAT_WOOD = 59;
constexpr uint8_t FIGURE_FIRST = PERSON_SKIN, FIGURE_LAST = BOAT_WOOD;

// Building materials of the .vox building models (assets/buildings): palette index = block ID, so
// the colors in MagicaVoxel are the colors in the game (table in shaders/render/shade.comp)
constexpr uint8_t MODEL_MATERIALS_FIRST = 60;
constexpr uint8_t PLASTER_WHITE = 60;
constexpr uint8_t PLASTER_CREAM = 61;
constexpr uint8_t PLASTER_OCHRE = 62;
constexpr uint8_t TIMBER_DARK = 63;
constexpr uint8_t TIMBER_LIGHT = 64;
constexpr uint8_t THATCH = 65;
constexpr uint8_t THATCH_DARK = 66;
constexpr uint8_t ROOF_TILE_RED = 67;
constexpr uint8_t ROOF_TILE_DARK = 68;
constexpr uint8_t ROOF_SLATE = 69;
constexpr uint8_t STONE_LIGHT = 70;
constexpr uint8_t STONE_DARK = 71;
constexpr uint8_t COBBLE = 72;
constexpr uint8_t WINDOW_GLASS = 73; // Glows warm at night
constexpr uint8_t WINDOW_FRAME = 74;
constexpr uint8_t DOOR_WOOD = 75;
constexpr uint8_t SHUTTER_GREEN = 76;
constexpr uint8_t SHUTTER_BLUE = 77;
constexpr uint8_t SHUTTER_RED = 78;
constexpr uint8_t CHIMNEY_BRICK = 79;
constexpr uint8_t FLOWER_RED = 80;
constexpr uint8_t FLOWER_YELLOW = 81;
constexpr uint8_t LEAVES = 82;
constexpr uint8_t FENCE_WOOD = 83;
constexpr uint8_t HAY = 84;
constexpr uint8_t CRATE = 85;
constexpr uint8_t BARREL = 86;
constexpr uint8_t IRON = 87;
constexpr uint8_t AWNING_RED = 88;
constexpr uint8_t AWNING_WHITE = 89;
constexpr uint8_t AWNING_BLUE = 90;
constexpr uint8_t AWNING_YELLOW = 91;
constexpr uint8_t WELL_WATER = 92;
constexpr uint8_t GARDEN_SOIL = 93;
constexpr uint8_t VEGETABLES = 94;
constexpr uint8_t FISH_SILVER = 95;
constexpr uint8_t WOOL_WHITE = 96;
constexpr uint8_t PIG_PINK = 97;
constexpr uint8_t SAUSAGE = 98;
constexpr uint8_t MUD = 99;
constexpr uint8_t MODEL_MATERIALS_LAST = 99;
} // namespace Block

// Grass tufts and water are drawn, but the player, raycasts and projectiles pass through them
inline bool IsSolidBlock(uint8_t id) {
    return id != Block::AIR && id != Block::GRASS_TUFT && id != Block::WATER;
}
