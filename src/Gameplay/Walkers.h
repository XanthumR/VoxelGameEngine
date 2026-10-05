#pragma once

#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class IslandEconomyManager;
class RoadNetwork;

// One drawn walker, 16 bytes to match the std430 layout in shaders/people/walkers.comp
struct WalkerFigure {
    glm::ivec4 feet; // xyz = voxel of the feet, w = packed look (see Pack)

    // bits 0-1 tier, 2-3 direction (0 +x, 1 -x, 2 +z, 3 -z), 4-5 walk frame, 6-8 look variant
    static int Pack(int tier, int direction, int frame, int variant) {
        return (tier & 3) | ((direction & 3) << 2) | ((frame & 3) << 4) | ((variant & 7) << 6);
    }
};

// Residents walking the roads. Purely visual (not part of the deterministic simulation): every
// island has one walker per RESIDENTS_PER_WALKER residents; they step out of houses next to a road,
// wander from road tile to road tile, and vanish again when the population drops or their road is
// removed. Positions are turned into voxel figures for WalkerRenderer every frame.
class WalkerSystem {
public:
    static constexpr int MAX_WALKERS = 512;
    static constexpr int RESIDENTS_PER_WALKER = 5;
    static constexpr float TILES_PER_SECOND = 5.0f / TILE_SIZE; // About 5 voxels per second
    static constexpr float SPAWN_INTERVAL = 0.3f;   // Seconds between walkers leaving houses, per island
    static constexpr float STRIDE = 2.5f;           // Voxels walked per frame of the walk cycle

    WalkerSystem();

    void Update(float deltaTime, const GameObjectRegistry& objects, const RoadNetwork& roads, const IslandEconomyManager& economy);

    const std::vector<WalkerFigure>& Figures() const { return m_Figures; }
    size_t Count() const { return m_Walkers.size(); }
    size_t CountOn(IslandId island) const;

    // Target number of walkers for an island with this many residents
    static int TargetCount(int residents) { return residents / RESIDENTS_PER_WALKER; }

private:
    struct Walker {
        IslandId island;
        glm::ivec2 tile;     // Road tile it is leaving
        glm::ivec2 next;     // Road tile it is walking to
        glm::ivec2 previous; // Where it came from (avoids turning back unless at a dead end)
        glm::ivec2 lane;     // Column inside a tile (one of two lanes on each axis), so walkers pass each other
        float progress;      // 0..1 from tile to next
        float walked;        // Voxels walked, drives the walk cycle
        uint8_t tier;
        uint8_t direction;   // Facing (0 +x, 1 -x, 2 +z, 3 -z); kept while standing still
        uint8_t variant;     // Skin, shirt and trousers choice (3 bits)
    };

    bool Spawn(IslandId island, const GameObjectRegistry& objects, const RoadNetwork& roads);
    glm::ivec2 ChooseNext(const Walker& walker, const RoadNetwork& roads);
    uint32_t Random();

    std::vector<Walker> m_Walkers;
    std::vector<WalkerFigure> m_Figures;
    std::vector<float> m_SpawnTimers;  // Per settled island (IslandEconomyManager order)
    uint32_t m_SpawnCursor = 0;        // Next object slot to look for a home in
    uint32_t m_RandomState = 0x9E3779B9u;
};
