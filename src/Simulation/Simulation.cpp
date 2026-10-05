#include "Simulation/Simulation.h"

Simulation::Simulation(TerrainGenerator& terrain) : m_Islands(terrain) {}

void Simulation::FixedUpdate(float tickSeconds) {
    // Warehouse reach and building connections, when roads or buildings changed
    m_Logistics.Update(m_Objects, m_Roads, m_BuildingsRevision);

    m_TickCount++;
    m_SimulationSeconds += tickSeconds;
}
