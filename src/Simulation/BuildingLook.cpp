#include "Simulation/BuildingLook.h"

#include <algorithm>

namespace {

// Details are sized for 4-column tiles and scaled up with the tile size
constexpr int DETAIL_SCALE = TILE_SIZE / 4;

} // namespace

glm::ivec2 FootprintTiles(const BuildingType& type, uint8_t rotation) {
    bool sideways = (rotation & 1) != 0; // Front faces +x or -x: the width runs along z
    return sideways ? glm::ivec2(type.footprintDepth, type.footprintWidth) : glm::ivec2(type.footprintWidth, type.footprintDepth);
}

glm::ivec2 FootprintColumns(const BuildingType& type, uint8_t rotation) {
    return FootprintTiles(type, rotation) * TILE_SIZE;
}

// Roof height above the ground at depth v: eaves overhang the walls by one column and the roof
// rises evenly to a ridge that runs parallel to the front and tops out at the type's height
static int RoofY(const BuildingType& type, int v) {
    int depth = type.footprintDepth * TILE_SIZE;
    int halfDepth = (depth - 1) / 2;
    int rise = type.height - type.wallHeight;
    return type.wallHeight - 1 + (std::min(v, depth - 1 - v) * rise + halfDepth / 2) / halfDepth;
}

int BuildingHeight(const BuildingType& type) {
    return type.height;
}

void BuildLook(const BuildingType& type, uint8_t rotation, std::vector<uint8_t>& ids) {
    const int width = type.footprintWidth * TILE_SIZE; // Local u: along the front
    const int depth = type.footprintDepth * TILE_SIZE; // Local v: from the front (0) to the back
    const int height = BuildingHeight(type);
    const glm::ivec2 size = FootprintColumns(type, rotation);
    ids.assign((size_t)size.x * size.y * height, Block::AIR);

    // Local (u, v) to the footprint column, turning the front to face the rotation's direction
    auto set = [&](int u, int v, int y, uint8_t id) {
        if (y < 0 || y >= height) return;
        glm::ivec2 column = RotateToFootprint(u, v, rotation, size);
        ids[(size_t)column.x + (size_t)size.x * ((size_t)column.y + (size_t)size.y * (size_t)y)] = id;
    };

    const int u0 = 1, u1 = width - 2, v0 = 1, v1 = depth - 2;
    const int doorWidth = 2 * DETAIL_SCALE, doorHeight = 3 * DETAIL_SCALE;
    const int windowWidth = DETAIL_SCALE, windowHeight = 2 * DETAIL_SCALE;

    if (type.style == LookStyle::Stall) {
        // Market stall: corner posts holding the awning, a counter round the sides with an
        // opening in the middle of the front, crates inside
        for (int y = 0; y < type.wallHeight; y++) {
            set(u0, v0, y, Block::WOOD);
            set(u1, v0, y, Block::WOOD);
            set(u0, v1, y, Block::WOOD);
            set(u1, v1, y, Block::WOOD);
        }
        const int opening = 4 * DETAIL_SCALE;
        for (int y = 0; y < DETAIL_SCALE; y++) {
            for (int u = u0 + 1; u < u1; u++) {
                if (u < width / 2 - opening / 2 || u >= width / 2 + opening / 2) set(u, v0, y, type.wallBlock);
                set(u, v1, y, type.wallBlock);
            }
            for (int v = v0 + 1; v < v1; v++) {
                set(u0, v, y, type.wallBlock);
                set(u1, v, y, type.wallBlock);
            }
        }
        for (int u = u0 + 2 * DETAIL_SCALE; u + DETAIL_SCALE <= u1 - DETAIL_SCALE; u += 3 * DETAIL_SCALE) {
            for (int du = 0; du < DETAIL_SCALE; du++) {
                for (int dv = 0; dv < DETAIL_SCALE; dv++) {
                    for (int y = 0; y < DETAIL_SCALE; y++) set(u + du, depth / 2 + dv, y, Block::WOOD); // Crates
                }
            }
        }
    } else {
        // Walls: the footprint inset by one column (the eaves overhang that ring), wooden corner
        // posts; a two-storey house has a masonry ground floor
        const int groundFloor = type.wallHeight / 2;
        for (int y = 0; y < type.wallHeight; y++) {
            uint8_t wall = (type.style == LookStyle::TwoStorey && y < groundFloor) ? Block::STONE_WALL : type.wallBlock;
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

        // Door in the middle of the front, windows either side on every floor, front and back
        for (int y = 0; y < doorHeight; y++) {
            for (int u = width / 2 - doorWidth / 2; u < width / 2 + doorWidth / 2; u++) set(u, v0, y, Block::WOOD);
        }
        int floors = type.style == LookStyle::TwoStorey ? 2 : 1;
        int floorHeight = type.wallHeight / floors;
        for (int floor = 0; floor < floors; floor++) {
            int windowBottom = floor * floorHeight + floorHeight / 3;
            for (int y = windowBottom; y < std::min(windowBottom + windowHeight, type.wallHeight - 1); y++) {
                for (int left : { u0 + 2 * DETAIL_SCALE, u1 - 2 * DETAIL_SCALE - windowWidth + 1 }) {
                    for (int u = left; u < left + windowWidth; u++) {
                        set(u, v0, y, Block::WOOD);
                        set(u, v1, y, Block::WOOD);
                    }
                }
            }
        }
    }

    // Roof, two voxels thick so no ray slips between the steps
    for (int v = 0; v < depth; v++) {
        int roofY = RoofY(type, v);
        for (int u = 0; u < width; u++) {
            set(u, v, roofY, type.roofBlock);
            if (v > 0 && v < depth - 1) set(u, v, roofY - 1, type.roofBlock);
        }
    }
}
