#pragma once

#include <cstdint>

// Fixed-step clock for the game simulation. The frame loop feeds it the real frame time and runs
// the simulation once per returned step, so game logic always advances in TICK_SECONDS steps no
// matter how fast or slow frames are.
class GameClock {
public:
    static constexpr float TICK_RATE = 10.0f; // Simulation steps per second
    static constexpr double TICK_SECONDS = 1.0 / TICK_RATE;
    static constexpr int MAX_STEPS_PER_FRAME = 5; // A long stall drops time instead of spiralling

    // Adds the frame's elapsed time and returns how many simulation steps to run now
    int StepsToRun(double frameDelta);

    // How far (0..1) the clock is between the last step and the next; for interpolating visuals
    float Alpha() const { return (float)(m_Accumulator / TICK_SECONDS); }

    // Simulation time that was skipped because a frame needed more than MAX_STEPS_PER_FRAME steps
    uint64_t DroppedSteps() const { return m_DroppedSteps; }

private:
    double m_Accumulator = 0.0;
    uint64_t m_DroppedSteps = 0;
};
