#pragma once

#include "Simulation/GameObjects.h"

#include <cstdint>
#include <vector>

class IslandEconomyManager;

// Residents and their needs, one step per simulation tick (see PopulationNeeds.h for the numbers):
//  - once per second, per island and tier in order (Farmers first): served residents consume their
//    goods from island storage; how much of what was due got delivered becomes the island's
//    smoothed supply of each need
//  - every tick, per house: need supply (service needs from being in a marketplace's reach, goods
//    from the island supply), residents moving towards what the supply allows, and the tier
//    upgrade (full house, every need met, planks) or downgrade (too few residents for too long)
// All integer math, so a run is reproducible.
class PopulationSystem {
public:
    static constexpr int CONSUMPTION_INTERVAL = 10; // Ticks between consumption cycles (1 s)
    static constexpr int GROWTH_INTERVAL = 20;      // Ticks per resident moving in or out (2 s)
    static constexpr int UPGRADE_TICKS = 100;       // A house must qualify this long to upgrade (10 s)
    static constexpr int DOWNGRADE_TICKS = 300;     // ... and fall short this long to downgrade (30 s)
    static constexpr int UPGRADE_SUPPLY = 900;      // Per mille every need must reach to upgrade
    static constexpr int32_t OWED_PER_GOOD = 60000; // Owed units per good (per-minute rates, per-second cycles)

    PopulationSystem();

    // tick: the simulation tick number (consumption runs when it is a multiple of CONSUMPTION_INTERVAL)
    void Update(GameObjectRegistry& objects, IslandEconomyManager& economy, uint64_t tick);

    // Houses whose building type changed (upgrade or downgrade) since the last clear: their look
    // must be rebuilt
    const std::vector<GameObjectId>& LookChanges() const { return m_LookChanges; }
    void ClearLookChanges() { m_LookChanges.clear(); }

    uint32_t Upgrades() const { return m_Upgrades; }
    uint32_t Downgrades() const { return m_Downgrades; }

private:
    void Consume(const GameObjectRegistry& objects, IslandEconomyManager& economy);
    void UpdateHouse(GameObjectRegistry& objects, IslandEconomyManager& economy, GameObjectId id);
    void PushLookChange(GameObjectId id);

    std::vector<GameObjectId> m_LookChanges;
    uint32_t m_Upgrades = 0;
    uint32_t m_Downgrades = 0;
};
