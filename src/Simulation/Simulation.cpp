#include "Simulation/Simulation.h"

Simulation::Simulation(TerrainGenerator& terrain) : m_Islands(terrain) {}

void Simulation::FixedUpdate(float tickSeconds) {
    m_TickCount++;
    m_SimulationSeconds += tickSeconds;
}
