#include "Simulation/BuildingLook.h"

#include <algorithm>

glm::ivec2 FootprintTiles(const BuildingType& type, uint8_t rotation) {
    bool sideways = (rotation & 1) != 0; // Front faces +x or -x: the width runs along z
    return sideways ? glm::ivec2(type.footprintDepth, type.footprintWidth) : glm::ivec2(type.footprintWidth, type.footprintDepth);
}

glm::ivec2 FootprintColumns(const BuildingType& type, uint8_t rotation) {
    return FootprintTiles(type, rotation) * TILE_SIZE;
}

// Roof height above the ground at depth v: eaves overhang the walls by one column and the
// roof rises one voxel per column to the ridge, which runs parallel to the front
static int RoofY(const BuildingType& type, int v) {
    int depth = type.footprintDepth * TILE_SIZE;
    return type.wallHeight - 1 + std::min(v, depth - 1 - v);
}

int BuildingHeight(const BuildingType& type) {
    int depth = type.footprintDepth * TILE_SIZE;
    return RoofY(type, (depth - 1) / 2) + 1;
}

void BuildLook(const BuildingType& type, uint8_t rotation, std::vector<uint8_t>& ids) {
    const int width = type.footprintWidth * TILE_SIZE; // Local u: along the front
    const int depth = type.footprintDepth * TILE_SIZE; // Local v: from the front (0) to the back
    const int height = BuildingHeight(type);
    const glm::ivec2 size = FootprintColumns(type, rotation);
    ids.assign((size_t)size.x * size.y * height, Block::AIR);

    // Local (u, v) to the footprint column, turning the front to face the rotation's direction
    auto set = [&](int u, int v, int y, uint8_t id) {
        int x = 0, z = 0;
        switch (rotation & 3) {
        case 0: x = u;              z = v;              break;
        case 1: x = size.x - 1 - v; z = u;              break;
        case 2: x = size.x - 1 - u; z = size.y - 1 - v; break;
        default: x = v;             z = size.y - 1 - u; break;
        }
        ids[(size_t)x + (size_t)size.x * ((size_t)z + (size_t)size.y * (size_t)y)] = id;
    };

    const int u0 = 1, u1 = width - 2, v0 = 1, v1 = depth - 2;

    if (type.style == LookStyle::Stall) {
        // Market stall: corner posts holding the awning, a waist-high counter round the sides with
        // an opening in the middle of the front, crates inside
        for (int y = 0; y < type.wallHeight; y++) {
            set(u0, v0, y, Block::WOOD);
            set(u1, v0, y, Block::WOOD);
            set(u0, v1, y, Block::WOOD);
            set(u1, v1, y, Block::WOOD);
        }
        for (int u = u0 + 1; u < u1; u++) {
            if (u < width / 2 - 2 || u > width / 2 + 1) set(u, v0, 0, type.wallBlock);
            set(u, v1, 0, type.wallBlock);
        }
        for (int v = v0 + 1; v < v1; v++) {
            set(u0, v, 0, type.wallBlock);
            set(u1, v, 0, type.wallBlock);
        }
        for (int u = u0 + 2; u <= u1 - 2; u += 3) set(u, depth / 2, 0, Block::WOOD); // Crates
    } else {
        // Walls: the footprint inset by one column (the eaves overhang that ring), wooden corner
        // posts; a two-storey house has a masonry ground floor
        for (int y = 0; y < type.wallHeight; y++) {
            uint8_t wall = (type.style == LookStyle::TwoStorey && y < 3) ? Block::STONE_WALL : type.wallBlock;
            for (int u = u0; u <= u1; u++) {
                set(u, v0, y, wall);
                set(u, v1, y, wall);
            }
            for (int v = v0; v <= v1; v++) {
                set(u0, v, y, wall);
                set(u1, v, y, wall);
            }
            set(u0, v0, y, Block::WOOD);
            set(u1, v0, y, Block::WOOD);
            set(u0, v1, y, Block::WOOD);
            set(u1, v1, y, Block::WOOD);
        }

        // Gable ends: wall up to the underside of the roof
        for (int v = v0 + 1; v < v1; v++) {
            for (int y = type.wallHeight; y < RoofY(type, v) - 1; y++) {
                set(u0, v, y, type.wallBlock);
                set(u1, v, y, type.wallBlock);
            }
        }

        // Door (2 wide, 3 tall) in the middle of the front, windows either side, front and back
        // (and on the upper floor of a two-storey house)
        for (int y = 0; y < 3; y++) {
            set(width / 2 - 1, v0, y, Block::WOOD);
            set(width / 2, v0, y, Block::WOOD);
        }
        for (int floorY : { 2, 5 }) {
            if (floorY == 5 && type.style != LookStyle::TwoStorey) continue;
            for (int y = floorY; y < std::min(floorY + 2, type.wallHeight - 1); y++) {
                for (int u : { u0 + 2, u1 - 2 }) {
                    set(u, v0, y, Block::WOOD);
                    set(u, v1, y, Block::WOOD);
                }
            }
        }
    }

    // Roof, two voxels thick so no ray slips between the diagonal steps
    for (int v = 0; v < depth; v++) {
        int roofY = RoofY(type, v);
        for (int u = 0; u < width; u++) {
            set(u, v, roofY, type.roofBlock);
            if (v > 0 && v < depth - 1) set(u, v, roofY - 1, type.roofBlock);
        }
    }
}
