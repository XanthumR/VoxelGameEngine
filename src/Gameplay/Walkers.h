#pragma once

#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class IslandEconomyManager;
class RoadNetwork;

// One drawn walker, 16 bytes to match the std430 layout in shaders/people/walkers.comp
struct WalkerFigure {
    glm::ivec4 feet; // xyz = voxel of the feet, w = population tier (shirt color)
};

// Residents walking the roads. Purely visual (not part of the deterministic simulation): every
// island has one walker per RESIDENTS_PER_WALKER residents; they step out of houses next to a road,
// wander from road tile to road tile, and vanish again when the population drops or their road is
// removed. Positions are turned into voxel figures for WalkerRenderer every frame.
class WalkerSystem {
public:
    static constexpr int MAX_WALKERS = 512;
    static constexpr int RESIDENTS_PER_WALKER = 5;
    static constexpr float TILES_PER_SECOND = 0.8f; // About 3 voxels per second
    static constexpr float SPAWN_INTERVAL = 0.3f;   // Seconds between walkers leaving houses, per island

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
        glm::ivec2 lane;     // Column inside a tile (1 or 2 on each axis), so walkers do not all overlap
        float progress;      // 0..1 from tile to next
        uint8_t tier;
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
