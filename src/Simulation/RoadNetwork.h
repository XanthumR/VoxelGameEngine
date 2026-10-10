#pragma once

#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

// One road tile. The reach fields are filled in by the logistics pass (Simulation)
struct RoadTile {
    static constexpr uint16_t UNREACHED = 0xFFFF;
    uint16_t distance = UNREACHED;                // Road tiles from the nearest warehouse (1 = touching it)
    GameObjectId warehouse = INVALID_GAME_OBJECT; // That warehouse
    // Per service type (ServiceType): road tiles from the nearest connected building of that type, and that building
    std::array<uint16_t, SERVICE_COUNT> serviceDistance = AllUnreached();
    std::array<GameObjectId, SERVICE_COUNT> service = {};

    static constexpr std::array<uint16_t, SERVICE_COUNT> AllUnreached() {
        std::array<uint16_t, SERVICE_COUNT> distances{};
        for (uint16_t& distance : distances) distance = UNREACHED;
        return distances;
    }
};

// The set of build tiles (see TILE_SIZE) that are road. Only the data: the road voxels are written
// by the road tool. Revision() changes on every add or remove, so derived data (warehouse reach,
// the tile overlay) knows when to rebuild.
class RoadNetwork {
public:
    static constexpr size_t RESERVED_TILES = 65536;

    RoadNetwork();

    bool Add(glm::ivec2 tile);    // False if already road
    bool Remove(glm::ivec2 tile); // False if not road
    bool IsRoad(glm::ivec2 tile) const { return m_Tiles.count(TileKey(tile)) != 0; }
    RoadTile* Find(glm::ivec2 tile);
    const RoadTile* Find(glm::ivec2 tile) const;

    size_t Count() const { return m_Tiles.size(); }
    uint32_t Revision() const { return m_Revision; }

    template <typename Function>
    void ForEach(Function function) {
        for (auto& entry : m_Tiles) function(KeyToTile(entry.first), entry.second);
    }
    template <typename Function>
    void ForEach(Function function) const {
        for (const auto& entry : m_Tiles) function(KeyToTile(entry.first), entry.second);
    }

    static uint64_t TileKey(glm::ivec2 tile) { return ((uint64_t)(uint32_t)tile.x << 32) | (uint32_t)tile.y; }
    static glm::ivec2 KeyToTile(uint64_t key) { return glm::ivec2((int32_t)(uint32_t)(key >> 32), (int32_t)(uint32_t)key); }

private:
    std::unordered_map<uint64_t, RoadTile> m_Tiles;
    uint32_t m_Revision = 0;
};

// Tiles of an L-shaped road from a to b (both included): along the axis with the longer
// distance first, then the other. out is cleared first; reserve it to avoid allocating.
void MakeLPath(glm::ivec2 a, glm::ivec2 b, std::vector<glm::ivec2>& out);
