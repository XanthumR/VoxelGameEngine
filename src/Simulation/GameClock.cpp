#include "Simulation/GameClock.h"

int GameClock::StepsToRun(double frameDelta, int speed) {
    if (frameDelta > 0.0) m_Accumulator += frameDelta * speed;

    int steps = 0;
    while (m_Accumulator >= TICK_SECONDS && steps < MAX_STEPS_PER_FRAME * speed) {
        m_Accumulator -= TICK_SECONDS;
        steps++;
    }

    // Still behind after the cap: drop the whole steps, keep the fraction for Alpha()
    while (m_Accumulator >= TICK_SECONDS) {
        m_Accumulator -= TICK_SECONDS;
        m_DroppedSteps++;
    }
    return steps;
}
