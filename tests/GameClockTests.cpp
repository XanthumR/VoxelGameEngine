#include "Simulation/GameClock.h"
#include "Simulation/Simulation.h"
#include "TestWorld.h"

#include <gtest/gtest.h>

TEST(GameClockTest, NoStepUntilATickHasPassed) {
    GameClock clock;
    EXPECT_EQ(clock.StepsToRun(0.06), 0);
    EXPECT_EQ(clock.StepsToRun(0.06), 1); // 0.12 s accumulated
    EXPECT_GE(clock.Alpha(), 0.0f);
    EXPECT_LT(clock.Alpha(), 1.0f);
}

TEST(GameClockTest, TenStepsPerSecondAtAnyFrameRate) {
    for (double fps : { 30.0, 60.0, 144.0, 1000.0 }) {
        GameClock clock;
        int steps = 0;
        int frames = (int)(fps * 10.0); // 10 seconds
        for (int i = 0; i < frames; i++) steps += clock.StepsToRun(1.0 / fps);
        EXPECT_NEAR(steps, 100, 1) << "at " << fps << " FPS";
        EXPECT_EQ(clock.DroppedSteps(), 0u);
    }
}

TEST(GameClockTest, SlowFrameIsCappedAndTheRestDropped) {
    GameClock clock;
    EXPECT_EQ(clock.StepsToRun(2.05), GameClock::MAX_STEPS_PER_FRAME);
    EXPECT_EQ(clock.DroppedSteps(), 15u); // 20 steps were due
    EXPECT_LT(clock.Alpha(), 1.0f);      // The fraction is kept
    EXPECT_EQ(clock.StepsToRun(0.0), 0); // No backlog carried over
}

TEST(GameClockTest, NegativeTimeIsIgnored) {
    GameClock clock;
    EXPECT_EQ(clock.StepsToRun(-1.0), 0);
    EXPECT_EQ(clock.StepsToRun(0.1), 1);
}

TEST(SimulationTest, FixedUpdateCountsTicks) {
    Simulation simulation(TestWorld::Get().terrain);
    for (int i = 0; i < 25; i++) simulation.FixedUpdate((float)GameClock::TICK_SECONDS);
    EXPECT_EQ(simulation.TickCount(), 25u);
    EXPECT_NEAR(simulation.SimulationSeconds(), 2.5, 1e-5);
}
