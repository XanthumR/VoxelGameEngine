#include "World/Islands.h"

#include "ThirdParty/FastNoiseLite.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

// How many islands of each size (Small, Medium, Large, Extra large) each session has; the Old
// World's first Large island is the start island
constexpr std::array<std::array<int, 4>, SESSION_COUNT> ISLAND_COUNTS = { {
    { 5, 4, 3, 1 }, // Old World
    { 4, 3, 2, 0 }, // New World
} };
constexpr float WOBBLE = 0.4f;   // How far the coast strays from an oval, as a share of the radius
constexpr float BUMPS = 1.3f;    // Noise frequency per radius: bays and headlands round the coast
constexpr float MAX_STRETCH = 1.5f; // The longest island is this many times as long as wide (squared)
constexpr int PLACE_TRIES = 4000;// Random spots tried per island...
constexpr int PLACE_CHOICES = 16; // ...of which the first this many that fit are compared

uint32_t Mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

struct Random {
    uint32_t state;
    float Next() { // [0, 1)
        state = Mix(state + 0x9E3779B9u);
        return (float)(state >> 8) / 16777216.0f;
    }
};

// An island's land on a square raster round its middle, one 4-connected piece
struct Shape {
    int half = 0; // The raster is 2 half + 1 tiles square
    std::vector<uint8_t> land;
    int tiles = 0;
    template <typename Function>
    void ForEachLand(Function function) const {
        int side = 2 * half + 1;
        for (int z = 0; z < side; z++) {
            for (int x = 0; x < side; x++) {
                if (land[z * side + x]) function(glm::ivec2(x - half, z - half));
            }
        }
    }
};

// The outline: an oval at an angle, stretch times as long as wide (the same area as a disc of the
// radius), its coast pushed in and out by the noise
struct Outline {
    glm::vec2 offset; // Where in the noise
    float angle, stretch;
};

// A noisy oval of this radius, smoothed as the coast needs (a land tile with fewer than two land
// neighbours is sea, a sea tile with three or four is land: no lone tiles, spikes or notches), and
// only the piece joined to the middle kept
Shape MakeShape(const FastNoiseLite& noise, const Outline& outline, float radius) {
    Shape shape;
    glm::vec2 along(std::cos(outline.angle), std::sin(outline.angle));
    float stretch = std::sqrt(outline.stretch);
    shape.half = (int)std::ceil(radius * stretch * (1.0f + WOBBLE)) + 2;
    int side = 2 * shape.half + 1;
    std::vector<uint8_t> raw(side * side);
    for (int z = 0; z < side; z++) {
        for (int x = 0; x < side; x++) {
            glm::vec2 d = glm::vec2(x - shape.half, z - shape.half) / radius;
            glm::vec2 oval(glm::dot(d, along) / stretch, (d.y * along.x - d.x * along.y) * stretch);
            float n = std::clamp(noise.GetNoise(outline.offset.x + d.x * BUMPS, outline.offset.y + d.y * BUMPS), -1.0f, 1.0f);
            raw[z * side + x] = glm::length(oval) < 1.0f - WOBBLE * n;
        }
    }
    auto rawAt = [&](int x, int z) { return x >= 0 && z >= 0 && x < side && z < side && raw[z * side + x]; };
    auto smoothLand = [&](int x, int z) {
        int neighbours = rawAt(x + 1, z) + rawAt(x - 1, z) + rawAt(x, z + 1) + rawAt(x, z - 1);
        return rawAt(x, z) ? neighbours >= 2 : neighbours >= 3;
    };

    shape.land.assign(side * side, 0);
    std::vector<glm::ivec2> open = { glm::ivec2(shape.half) };
    shape.land[shape.half * side + shape.half] = 1;
    while (!open.empty()) {
        glm::ivec2 tile = open.back();
        open.pop_back();
        shape.tiles++;
        for (glm::ivec2 step : { glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1) }) {
            glm::ivec2 next = tile + step;
            if (next.x < 0 || next.y < 0 || next.x >= side || next.y >= side) continue;
            uint8_t& land = shape.land[next.y * side + next.x];
            if (!land && smoothLand(next.x, next.y)) {
                land = 1;
                open.push_back(next);
            }
        }
    }

    // Lakes (sea not joined to the open sea round the raster) become land
    std::vector<uint8_t> sea(side * side, 0);
    open.clear();
    for (int i = 0; i < side; i++) {
        for (glm::ivec2 edge : { glm::ivec2(i, 0), glm::ivec2(i, side - 1), glm::ivec2(0, i), glm::ivec2(side - 1, i) }) {
            if (!shape.land[edge.y * side + edge.x] && !sea[edge.y * side + edge.x]) {
                sea[edge.y * side + edge.x] = 1;
                open.push_back(edge);
            }
        }
    }
    while (!open.empty()) {
        glm::ivec2 tile = open.back();
        open.pop_back();
        for (glm::ivec2 step : { glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1) }) {
            glm::ivec2 next = tile + step;
            if (next.x < 0 || next.y < 0 || next.x >= side || next.y >= side) continue;
            int i = next.y * side + next.x;
            if (!shape.land[i] && !sea[i]) {
                sea[i] = 1;
                open.push_back(next);
            }
        }
    }
    for (int i = 0; i < side * side; i++) {
        if (!shape.land[i] && !sea[i]) {
            shape.land[i] = 1;
            shape.tiles++;
        }
    }
    return shape;
}

// The shape scaled until it has about target land tiles
Shape FitShape(const FastNoiseLite& noise, const Outline& outline, int target) {
    float radius = std::sqrt((float)target / 3.14159265f);
    Shape shape;
    for (int i = 0; i < 8; i++) {
        shape = MakeShape(noise, outline, radius);
        if (std::abs(shape.tiles - target) <= target / 100) break;
        radius *= std::sqrt((float)target / (float)shape.tiles);
    }
    return shape;
}

SessionIslands Build(int session) {
    constexpr int N = SESSION_TILES;
    const Session& info = SESSIONS[session];
    SessionIslands result;
    result.map.assign(N * N, 0);
    std::vector<uint8_t> reserved(N * N, 0); // Land and the sea within ISLAND_GAP of it
    std::vector<glm::ivec2> centers;         // Of the islands placed so far, local
    auto reserve = [&](glm::ivec2 local) {
        for (int z = std::max(0, local.y - ISLAND_GAP); z <= std::min(N - 1, local.y + ISLAND_GAP); z++) {
            for (int x = std::max(0, local.x - ISLAND_GAP); x <= std::min(N - 1, local.x + ISLAND_GAP); x++) reserved[z * N + x] = 1;
        }
    };
    for (int z = -TRADER_ISLAND_RADIUS.y; z <= TRADER_ISLAND_RADIUS.y; z++) {
        for (int x = -TRADER_ISLAND_RADIUS.x; x <= TRADER_ISLAND_RADIUS.x; x++) {
            glm::ivec2 local = info.traderTile + glm::ivec2(x, z);
            if (InTraderIsland(info.originTile + local)) reserve(local);
        }
    }
    centers.push_back(info.traderTile);

    FastNoiseLite noise(1234 + session);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(3);
    noise.SetFrequency(1.0f);
    Random random{ 0x5EED0000u + (uint32_t)session };

    auto fits = [&](const Shape& shape, glm::ivec2 center) {
        bool fit = true;
        shape.ForEachLand([&](glm::ivec2 d) {
            glm::ivec2 local = center + d;
            fit = fit && local.x >= EDGE_SEA_TILES && local.y >= EDGE_SEA_TILES && local.x < N - EDGE_SEA_TILES &&
                  local.y < N - EDGE_SEA_TILES && !reserved[local.y * N + local.x];
        });
        return fit;
    };
    auto place = [&](const Shape& shape, glm::ivec2 center, IslandSize size) {
        result.sites.push_back({ info.originTile + center, size, shape.tiles });
        shape.ForEachLand([&](glm::ivec2 d) { result.map[(center.y + d.y) * N + center.x + d.x] = (uint8_t)result.sites.size(); });
        shape.ForEachLand([&](glm::ivec2 d) { reserve(center + d); });
        centers.push_back(center);
    };

    // The biggest first, while there is room
    for (int size = (int)ISLAND_SIZES.size() - 1; size >= 0; size--) {
        const IslandSizeClass& range = ISLAND_SIZES[size];
        for (int i = 0; i < ISLAND_COUNTS[session][size]; i++) {
            int target = range.minTiles + (int)((0.25f + 0.5f * random.Next()) * (float)(range.maxTiles - range.minTiles));
            Outline outline{ glm::vec2(random.Next(), random.Next()) * 1000.0f, random.Next() * 3.14159265f,
                1.0f + (MAX_STRETCH * MAX_STRETCH - 1.0f) * random.Next() };
            Shape shape = FitShape(noise, outline, target);
            // The spot that fits farthest from the islands placed so far, so they spread over the map
            glm::ivec2 best(-1);
            int bestDistance = -1, choices = 0;
            for (int t = 0; t < PLACE_TRIES && choices < PLACE_CHOICES; t++) {
                int low = EDGE_SEA_TILES + shape.half, span = N - 2 * low;
                glm::ivec2 center = glm::ivec2(low) + glm::ivec2(glm::vec2(random.Next(), random.Next()) * (float)span);
                if (!fits(shape, center)) continue;
                choices++;
                int distance = N * N;
                for (glm::ivec2 other : centers) distance = std::min(distance, (center.x - other.x) * (center.x - other.x) + (center.y - other.y) * (center.y - other.y));
                if (distance > bestDistance) {
                    bestDistance = distance;
                    best = center;
                }
            }
            if (bestDistance < 0) continue;
            if (session == SESSION_OLD_WORLD && size == (int)IslandSize::Large && result.start < 0) result.start = (int)result.sites.size();
            place(shape, best, (IslandSize)size);
        }
    }
    return result;
}

} // namespace

const SessionIslands& IslandsOf(int session) {
    static const std::array<SessionIslands, SESSION_COUNT> islands = [] {
        std::array<SessionIslands, SESSION_COUNT> all;
        for (int i = 0; i < SESSION_COUNT; i++) all[i] = Build(i);
        return all;
    }();
    return islands[session];
}

glm::ivec2 SpawnSearchColumn() {
    const SessionIslands& islands = IslandsOf(SESSION_OLD_WORLD);
    return islands.sites[islands.start].center * TILE_SIZE + TILE_SIZE / 2;
}

bool IsIslandTile(glm::ivec2 tile) {
    if (InTraderIsland(tile)) return true;
    int session = SessionAtTile(tile);
    if (session < 0) return false;
    glm::ivec2 local = tile - SESSIONS[session].originTile;
    return IslandsOf(session).map[local.y * SESSION_TILES + local.x] != 0;
}
