#pragma once

#include "Economy/IslandEconomy.h"
#include "Economy/PopulationSystem.h"
#include "Economy/Production.h"
#include "Simulation/GameObjects.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/Logistics.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/Placement.h"
#include "Simulation/RoadNetwork.h"
#include "Simulation/TreeRegistry.h"

#include <cstdint>

class TerrainGenerator;
class VoxelWorld;

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
    RoadNetwork& Roads() { return m_Roads; }
    const RoadNetwork& Roads() const { return m_Roads; }
    IslandEconomyManager& Economy() { return m_Economy; }
    const IslandEconomyManager& Economy() const { return m_Economy; }
    LogisticsSystem& Logistics() { return m_Logistics; }
    const LogisticsSystem& Logistics() const { return m_Logistics; }
    PopulationSystem& Population() { return m_Population; }
    TreeRegistry& Trees() { return m_Trees; }
    ProductionSystem& Production() { return m_Production; }
    const ProductionSystem& Production() const { return m_Production; }
    const PopulationSystem& Population() const { return m_Population; }

    // Everything placement checks against, for this world
    PlacementContext MakePlacementContext(const VoxelWorld& world) { return { world, m_Islands, m_Occupancy, m_Roads }; }

    // Bumped whenever a building is placed or demolished, so derived data knows to rebuild
    uint32_t BuildingsRevision() const { return m_BuildingsRevision; }
    void MarkBuildingsChanged() { m_BuildingsRevision++; }

private:
    IslandRegistry m_Islands;
    GameObjectRegistry m_Objects;
    OccupancyGrid m_Occupancy;
    RoadNetwork m_Roads;
    IslandEconomyManager m_Economy;
    LogisticsSystem m_Logistics;
    PopulationSystem m_Population;
    TreeRegistry m_Trees;
    ProductionSystem m_Production;

    uint64_t m_TickCount = 0;
    double m_SimulationSeconds = 0.0;
    uint32_t m_BuildingsRevision = 0;
};
