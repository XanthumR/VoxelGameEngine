#pragma once

#include <cstdint>

// Owns the game systems and advances them in fixed steps (see GameClock). Everything that must be
// deterministic (production, population, economy) runs here, never in the frame loop.
class Simulation {
public:
    void FixedUpdate(float tickSeconds);

    uint64_t TickCount() const { return m_TickCount; }
    double SimulationSeconds() const { return m_SimulationSeconds; }

private:
    uint64_t m_TickCount = 0;
    double m_SimulationSeconds = 0.0;
};
