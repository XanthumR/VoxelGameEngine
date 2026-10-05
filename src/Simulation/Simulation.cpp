#include "Simulation/Simulation.h"

Simulation::Simulation(TerrainGenerator& terrain) : m_Islands(terrain) {}

void Simulation::FixedUpdate(float tickSeconds) {
    // Warehouse reach and building connections, when roads or buildings changed
    m_Logistics.Update(m_Objects, m_Roads, m_BuildingsRevision);
    // Residents: consumption, growth, upgrades
    m_Population.Update(m_Objects, m_Economy, m_TickCount);

    m_TickCount++;
    m_SimulationSeconds += tickSeconds;
}
