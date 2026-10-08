#include "Simulation/BuildingLook.h"

#include <algorithm>
#include <array>
#include <cmath>

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

void ConstructionLook(const std::vector<uint8_t>& look, glm::ivec3 size, float progress, std::vector<uint8_t>& out) {
    constexpr float RAGGED = 4.0f; // Layers the finished edge wanders up and down
    constexpr float FRAME = 6.0f;  // Layers of timber frame above it
    float front = progress * ((float)size.y + RAGGED + FRAME);
    out.assign(look.size(), Block::AIR);
    size_t i = 0;
    for (int y = 0; y < size.y; y++) {
        for (int z = 0; z < size.z; z++) {
            for (int x = 0; x < size.x; x++, i++) {
                if (i >= look.size() || look[i] == Block::AIR) continue;
                uint32_t hash = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;
                hash = (hash ^ (hash >> 13)) * 0x5bd1e995u;
                float level = (float)y + (float)((hash >> 8) & 255) / 256.0f * RAGGED;
                if (level < front - FRAME) out[i] = look[i];
                else if (level < front && (y % 4 == 0 || (x + z) % 4 == 0)) out[i] = Block::TIMBER_LIGHT; // Posts and beams on every wall
            }
        }
    }
}

void DemolitionLook(const std::vector<uint8_t>& look, glm::ivec3 size, int groundLayer, float progress, std::vector<uint8_t>& out) {
    constexpr float FALL_START = 0.45f;  // When the bottom layer gives way; the top goes at once
    constexpr float FALL_JITTER = 0.15f; // Voxels of a layer let go up to this much apart
    constexpr float FALL_TIME = 0.35f;   // Progress a voxel takes to fall the whole height
    constexpr float RUBBLE_GROWN = 0.6f; // The mound is at full height from here...
    constexpr float RUBBLE_SINKS = 0.8f; // ...and sinks away from here
    out.assign(look.size(), Block::AIR);
    if (progress <= 0.0f) {
        out = look;
        return;
    }
    if (progress >= 1.0f || look.size() < (size_t)size.x * size.y * size.z) return;

    const size_t layer = (size_t)size.x * size.z;
    std::copy(look.begin(), look.begin() + layer * groundLayer, out.begin()); // Pilings stay
    const int height = std::max(1, size.y - groundLayer);
    const float gravity = 2.0f * (float)height / (FALL_TIME * FALL_TIME);
    const int maxRubble = std::clamp(height / 5, 2, 6);
    auto hashOf = [](int x, int y, int z) {
        uint32_t hash = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;
        return (hash ^ (hash >> 13)) * 0x5bd1e995u;
    };

    // The rubble: the building's commonest material, flecked with dark stone; a mound highest in the middle
    std::array<int, 256> counts = {};
    for (size_t i = layer * groundLayer; i < look.size(); i++) counts[look[i]]++;
    counts[Block::AIR] = 0;
    uint8_t material = (uint8_t)(std::max_element(counts.begin(), counts.end()) - counts.begin());
    if (material == Block::AIR) material = Block::STONE_DARK;
    const float grown = std::min(1.0f, progress / RUBBLE_GROWN);
    const float sunk = progress < RUBBLE_SINKS ? 1.0f : (1.0f - progress) / (1.0f - RUBBLE_SINKS);
    auto rubbleAt = [&](int x, int z) {
        float u = std::abs(((float)x + 0.5f) / (float)size.x * 2.0f - 1.0f);
        float v = std::abs(((float)z + 0.5f) / (float)size.z * 2.0f - 1.0f);
        float shape = 0.35f + 0.65f * (1.0f - std::max(u, v)) + (float)(hashOf(x, 0, z) >> 24 & 3) * 0.1f;
        return std::max(1, (int)((float)maxRubble * shape * grown * sunk + 0.5f));
    };
    for (int z = 0; z < size.z; z++) {
        for (int x = 0; x < size.x; x++) {
            int rubble = std::min(rubbleAt(x, z), height);
            for (int r = 0; r < rubble; r++) {
                out[(size_t)x + (size_t)size.x * ((size_t)z + (size_t)size.z * (size_t)(groundLayer + r))] =
                    hashOf(x, r, z) % 3 == 0 ? Block::STONE_DARK : material;
            }
        }
    }

    // Falling voxels; those that reach the mound are part of it
    size_t i = layer * groundLayer;
    for (int y = groundLayer; y < size.y; y++) {
        float start = FALL_START * (1.0f - (float)(y - groundLayer) / (float)height);
        for (int z = 0; z < size.z; z++) {
            for (int x = 0; x < size.x; x++, i++) {
                if (look[i] == Block::AIR) continue;
                float t = progress - start - FALL_JITTER * (float)((hashOf(x, y, z) >> 8) & 255) / 256.0f;
                int fallen = y - (t > 0.0f ? (int)(0.5f * gravity * t * t) : 0);
                if (fallen < groundLayer + rubbleAt(x, z)) continue;
                out[(size_t)x + (size_t)size.x * ((size_t)z + (size_t)size.z * (size_t)fallen)] = look[i];
            }
        }
    }
}
