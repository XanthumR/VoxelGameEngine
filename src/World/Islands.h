#pragma once

#include "World/Sessions.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

// The islands of each session, as in Anno 1800: every island has a size class with a range of
// buildable (land) tiles, and each session's map holds a fixed mix of them. The layout is worked
// out once from fixed seeds, so it is the same in every game and on every thread: each island is
// a noisy blob scaled until its land tiles (one connected piece) come within its size's range,
// then placed at the best of a few random spots with ISLAND_GAP tiles of sea to any other island.
enum class IslandSize : uint8_t { Small, Medium, Large, ExtraLarge };
struct IslandSizeClass {
    const char* name;
    int minTiles, maxTiles; // Land tiles
};
constexpr std::array<IslandSizeClass, 4> ISLAND_SIZES = { {
    { "Small", 1200, 2500 },       // Outposts for one raw good
    { "Medium", 3000, 4500 },      // Secondary production, satellite towns
    { "Large", 5500, 7500 },       // A main island: long chains, big cities
    { "Extra large", 8000, 9500 }, // Rare
} };

constexpr int ISLAND_GAP = 10; // Sea tiles at least between two islands

struct IslandSite {
    glm::ivec2 center; // World tile, always land
    IslandSize size;
    int tiles;         // Land tiles
};
struct SessionIslands {
    std::vector<IslandSite> sites;
    std::vector<uint8_t> map; // SESSION_TILES^2: the site's index + 1 on its land tiles, 0 for sea
    int start = -1;           // The site the player starts on: the Old World's first Large island
};

// The session's islands (the trader's island is not among them: see InTraderIsland)
const SessionIslands& IslandsOf(int session);
// The middle of the start island, where the spawn search starts
glm::ivec2 SpawnSearchColumn();
// Land: an island's tile or the trader island's
bool IsIslandTile(glm::ivec2 tile);
