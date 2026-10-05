#include "Simulation/Simulation.h"

Simulation::Simulation(TerrainGenerator& terrain) : m_Islands(terrain), m_Trees(terrain) {}

void Simulation::FixedUpdate(float tickSeconds) {
    // Warehouse reach and building connections, when roads or buildings changed
    m_Logistics.Update(m_Objects, m_Roads, m_BuildingsRevision);
    // Residents: consumption, growth, upgrades
    m_Population.Update(m_Objects, m_Economy, m_TickCount);
    // Felled trees grow back
    m_Trees.Update(m_TickCount, m_Occupancy, m_Roads);

    m_TickCount++;
    m_SimulationSeconds += tickSeconds;
}
