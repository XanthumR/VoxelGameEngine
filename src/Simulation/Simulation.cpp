#include "Simulation/Simulation.h"

Simulation::Simulation(TerrainGenerator& terrain) : m_Islands(terrain), m_Trees(terrain), m_Ships(terrain) {}

void Simulation::FixedUpdate(float tickSeconds) {
    // Warehouse reach and building connections, when roads or buildings changed
    m_Logistics.Update(m_Objects, m_Roads, m_BuildingsRevision);
    // Residents: consumption, growth, upgrades
    m_Population.Update(m_Objects, m_Economy, m_TickCount);
    // Felled trees grow back
    m_Trees.Update(m_TickCount, m_Occupancy, m_Roads);
    // Producers: workforce, location, cycles. Their location factors follow buildings, roads and trees.
    uint32_t worldRevision = m_BuildingsRevision * 2654435761u + m_Roads.Revision() * 40503u + m_Trees.Revision();
    m_Production.Update(m_Objects, m_Economy, m_Islands, m_Occupancy, m_Roads, m_Trees, worldRevision, m_TickCount);
    // Taxes in, upkeep out
    m_Treasury.Update(m_Objects);
    // Ships sail on
    m_Ships.Update(m_Objects);

    m_TickCount++;
    m_SimulationSeconds += tickSeconds;
}
