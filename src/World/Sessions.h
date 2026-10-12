#pragma once

#include "World/WorldConstants.h"

#include <glm/glm.hpp>

#include <array>

// The worlds (sessions, as Anno calls them): each a fixed square map of build tiles with its own
// islands, all in the one voxel space but far enough apart that only one is ever streamed in. The
// sea goes on past a map's edge, but no island lies within EDGE_SEA_TILES of it and the camera
// stays over the map. Ships cross from one session to another by sailing off the edge.
struct Session {
    const char* name;
    glm::ivec2 originTile; // The map's first tile (its lowest x and z)
    bool tropical;         // Palms instead of broadleaf trees, beaches more than cliffs
    glm::ivec2 traderTile; // The middle of the trader's island (tiles from the map's first tile)
};

constexpr int SESSION_TILES = 512;      // A map's side in build tiles (6144 voxel columns)
constexpr int EDGE_SEA_TILES = 16;      // Open sea along every edge
constexpr int SESSION_OLD_WORLD = 0;
constexpr int SESSION_NEW_WORLD = 1;
constexpr std::array<Session, 2> SESSIONS = { {
    { "Old World", glm::ivec2(-276, -132), false, glm::ivec2(290, 236) },
    { "New World", glm::ivec2(1340, -160), true, glm::ivec2(250, 250) }, // 16 000 columns east: beyond any view distance
} };
constexpr int SESSION_COUNT = (int)SESSIONS.size();

// The session whose map holds this tile, -1 for the open sea between them
constexpr int SessionAtTile(glm::ivec2 tile) {
    for (int i = 0; i < SESSION_COUNT; i++) {
        glm::ivec2 local = tile - SESSIONS[i].originTile;
        if (local.x >= 0 && local.y >= 0 && local.x < SESSION_TILES && local.y < SESSION_TILES) return i;
    }
    return -1;
}
constexpr int SessionAtColumn(int wx, int wz) {
    return SessionAtTile(glm::ivec2(ColumnToTile(wx), ColumnToTile(wz)));
}
// Tiles from a tile to the nearest edge of its session's map (-1 outside every map)
constexpr int TilesToEdge(glm::ivec2 tile) {
    int session = SessionAtTile(tile);
    if (session < 0) return -1;
    glm::ivec2 local = tile - SESSIONS[session].originTile;
    int a = local.x < local.y ? local.x : local.y;
    int b = SESSION_TILES - 1 - (local.x > local.y ? local.x : local.y);
    return a < b ? a : b;
}
// The trader's island: a fixed oval of land in the open sea, the same in every game (the islands
// round it come from noise). TRADER_ISLAND_RADIUS is its half size in tiles along x and z.
constexpr glm::ivec2 TRADER_ISLAND_RADIUS(7, 5);
constexpr bool InTraderIsland(glm::ivec2 tile) {
    int session = SessionAtTile(tile);
    if (session < 0) return false;
    glm::ivec2 d = tile - SESSIONS[session].originTile - SESSIONS[session].traderTile;
    return d.x * d.x * TRADER_ISLAND_RADIUS.y * TRADER_ISLAND_RADIUS.y + d.y * d.y * TRADER_ISLAND_RADIUS.x * TRADER_ISLAND_RADIUS.x <=
           TRADER_ISLAND_RADIUS.x * TRADER_ISLAND_RADIUS.x * TRADER_ISLAND_RADIUS.y * TRADER_ISLAND_RADIUS.y;
}

// The middle of a session's map, in world columns
constexpr glm::ivec2 SessionCenterColumn(int session) {
    return (SESSIONS[session].originTile + SESSION_TILES / 2) * TILE_SIZE;
}
