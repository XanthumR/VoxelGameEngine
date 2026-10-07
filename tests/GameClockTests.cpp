#include "Simulation/BuildingLook.h"
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

TEST(GameClockTest, SpeedScalesTheSteps) {
    GameClock normal, fast, paused;
    int normalSteps = 0, fastSteps = 0, pausedSteps = 0;
    for (int i = 0; i < 600; i++) { // 10 s at 60 FPS
        normalSteps += normal.StepsToRun(1.0 / 60.0, 1);
        fastSteps += fast.StepsToRun(1.0 / 60.0, 4);
        pausedSteps += paused.StepsToRun(1.0 / 60.0, 0);
    }
    EXPECT_NEAR(normalSteps, 100, 1);
    EXPECT_NEAR(fastSteps, 400, 1);
    EXPECT_EQ(pausedSteps, 0);
    EXPECT_EQ(fast.DroppedSteps(), 0u);
}

TEST(GameClockTest, StepCapGrowsWithSpeed) {
    GameClock clock;
    EXPECT_EQ(clock.StepsToRun(1.0, 4), GameClock::MAX_STEPS_PER_FRAME * 4); // 40 steps were due
}

TEST(SimulationTest, MovedBuildingKeepsItsObjectAndTakesNewTiles) {
    Simulation simulation(TestWorld::Get().terrain);
    GameObjectRegistry& objects = simulation.Objects();
    GameObjectId id = objects.Create();
    objects.Building(id).type = BUILDING_SAWMILL;
    const glm::ivec2 from(10, 10), to(20, 14);
    simulation.PlaceLiftedBuilding(id, from, 0); // Set down the first time
    ProductionComponent& production = objects.Production(id);
    production.output = 1;
    production.cartOutput = 3; // Its cart is out on the road
    production.cartState = CartState::ToWarehouse;
    production.cartPosition = 2500;

    uint32_t revision = simulation.BuildingsRevision();
    simulation.LiftBuilding(id);
    EXPECT_EQ(simulation.Occupancy().At(from), INVALID_GAME_OBJECT);
    simulation.PlaceLiftedBuilding(id, to, 1);
    EXPECT_GT(simulation.BuildingsRevision(), revision);

    glm::ivec2 tiles = FootprintTiles(BUILDING_TYPES[BUILDING_SAWMILL], 1);
    EXPECT_EQ(simulation.Occupancy().At(to), id);
    EXPECT_EQ(simulation.Occupancy().At(to + tiles - 1), id);
    EXPECT_EQ(objects.Anchor(id).origin, glm::ivec3(to.x * TILE_SIZE, BUILD_GROUND_Y, to.y * TILE_SIZE));
    EXPECT_EQ(objects.Anchor(id).footprint, tiles * TILE_SIZE);
    EXPECT_EQ(objects.Building(id).rotation, 1);
    EXPECT_EQ(production.cartState, CartState::Idle); // Home with its load
    EXPECT_EQ(production.output, 4);
}
