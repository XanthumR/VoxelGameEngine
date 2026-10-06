#pragma once

#include "Simulation/GameObjects.h"
#include "Simulation/ItemType.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

class IslandEconomyManager;
class OccupancyGrid;
class TerrainGenerator;
class Treasury;

// Handle to a ship: low 16 bits the slot, high 16 bits its generation (as GameObjectId)
using ShipId = uint32_t;
constexpr ShipId INVALID_SHIP = 0xFFFFFFFFu;

enum class ShipState : uint8_t {
    Idle,    // Anchored at sea
    Sailing, // Following its path
    Docked,  // At a harbor's berth: cargo can move to and from that island
};

struct CargoSlot {
    ItemType item = ItemType::Wood;
    int amount = 0; // 0 = empty
};

struct Ship {
    glm::vec2 position{ 0.0f }; // Tiles (a tile's middle is +0.5)
    glm::vec2 previous{ 0.0f }; // Last tick's position, for drawing between ticks
    ShipState state = ShipState::Idle;
    GameObjectId harbor = INVALID_GAME_OBJECT; // Docked at, or sailing to
    std::array<CargoSlot, 2> cargo{};
    int pathLength = 0, pathIndex = 0;         // Waypoints in the ship system's path store
};

// The player's ships. They sail the open sea on the build-tile grid: Sea and Cliff tiles
// (TerrainGenerator::TileKindAt) that no building stands on are water, so a route can cross the
// whole map without loaded chunks. Routes are found with A* (8 neighbours, no corner cutting) in a
// window allocated once, then straightened where the water allows. Ships are built at a harbor and
// dock at its berth, the tile just past the end of its pier, where cargo moves to and from the
// island's storage. Advanced once per simulation tick.
class ShipSystem {
public:
    static constexpr int MAX_SHIPS = 32;
    static constexpr int MAX_PATH = 1024;   // Waypoints a route may have
    static constexpr int SEARCH_SIZE = 512; // Tiles across the A* window
    static constexpr float SPEED = 0.15f;   // Tiles per tick (1.5 a second)
    static constexpr int SLOT_CAPACITY = 50;
    static constexpr int SHIP_COINS = 500, SHIP_PLANKS = 20;

    explicit ShipSystem(TerrainGenerator& terrain);

    void Update(const GameObjectRegistry& objects);

    // A new ship docked at the harbor, paid with coins and the harbor island's planks; INVALID_SHIP
    // when the harbor is not one or the cost cannot be paid
    ShipId Build(GameObjectId harbor, const GameObjectRegistry& objects, IslandEconomyManager& economy, Treasury& treasury);
    // Sends a ship to a sea tile, or to a harbor's berth when harbor is given; false when no way by
    // sea leads there (the ship keeps what it was doing)
    bool SailTo(ShipId id, glm::ivec2 tile, const GameObjectRegistry& objects, const OccupancyGrid& occupancy,
        GameObjectId harbor = INVALID_GAME_OBJECT);
    // Moves cargo between a docked ship and its harbor's island: amount > 0 onto the ship, < 0 off it.
    // Returns how much moved (with the same sign), limited by the slots and the storage.
    int Transfer(ShipId id, ItemType item, int amount, const GameObjectRegistry& objects, IslandEconomyManager& economy);

    bool IsAlive(ShipId id) const;
    const Ship& Get(ShipId id) const { return m_Ships[id & 0xFFFF]; }
    ShipId IdAtSlot(int slot) const;
    int Count() const;
    // The ship nearest a position (tiles) within maxTiles; INVALID_SHIP when none
    ShipId Nearest(glm::vec2 position, float maxTiles) const;

    // Open water a ship can sail on
    bool IsWater(glm::ivec2 tile, const OccupancyGrid& occupancy);
    // The tile where ships dock at a harbor: two tiles past the middle of its pier's end
    static glm::ivec2 BerthTile(const GameObjectRegistry& objects, GameObjectId harbor);
    // Waypoints (tiles) from start to goal over water, straightened; false when there is no way
    // in the search window. Neither end has to be water itself.
    bool FindPath(glm::ivec2 start, glm::ivec2 goal, const OccupancyGrid& occupancy, std::vector<glm::ivec2>& out);

private:
    bool LineIsWater(glm::ivec2 from, glm::ivec2 to, const OccupancyGrid& occupancy);

    TerrainGenerator& m_Terrain;
    std::array<Ship, MAX_SHIPS> m_Ships{};
    std::array<uint16_t, MAX_SHIPS> m_Generations{};
    std::array<bool, MAX_SHIPS> m_Alive{};
    std::vector<glm::ivec2> m_Paths; // MAX_PATH waypoints per ship

    // A* window: cost so far, the step that reached each tile, and the open heap
    std::vector<uint32_t> m_Cost;
    std::vector<uint8_t> m_From;
    std::vector<uint64_t> m_Open; // (f << 32) | tile index, as a min-heap
    std::vector<glm::ivec2> m_Scratch; // The raw route while straightening it
    std::vector<glm::ivec2> m_Found;   // The last route found for an order
};
