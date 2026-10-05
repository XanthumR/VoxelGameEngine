#include "Simulation/Simulation.h"

void Simulation::FixedUpdate(float tickSeconds) {
    m_TickCount++;
    m_SimulationSeconds += tickSeconds;
}
