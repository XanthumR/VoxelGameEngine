#pragma once

#include "Simulation/GameObjects.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/OccupancyGrid.h"

#include <cstdint>

class TerrainGenerator;

// Owns the game systems and advances them in fixed steps (see GameClock). Everything that must be
// deterministic (production, population, economy) runs here, never in the frame loop.
class Simulation {
public:
    explicit Simulation(TerrainGenerator& terrain);

    void FixedUpdate(float tickSeconds);

    uint64_t TickCount() const { return m_TickCount; }
    double SimulationSeconds() const { return m_SimulationSeconds; }

    IslandRegistry& Islands() { return m_Islands; }
    GameObjectRegistry& Objects() { return m_Objects; }
    const GameObjectRegistry& Objects() const { return m_Objects; }
    OccupancyGrid& Occupancy() { return m_Occupancy; }
    const OccupancyGrid& Occupancy() const { return m_Occupancy; }

private:
    IslandRegistry m_Islands;
    GameObjectRegistry m_Objects;
    OccupancyGrid m_Occupancy;

    uint64_t m_TickCount = 0;
    double m_SimulationSeconds = 0.0;
};
