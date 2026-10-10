#include "Economy/IslandEconomy.h"
#include "Economy/Treasury.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Notifications.h"
#include "Simulation/Ships.h"
#include "TestWorld.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

constexpr IslandId ISLAND = 1;

struct NotificationFixture {
    GameObjectRegistry objects;
    IslandEconomyManager economy;
    ShipSystem ships{ TestWorld::Get().terrain };
    Treasury treasury;
    NotificationSystem notifications;
    uint64_t tick = 0;

    NotificationFixture() { economy.OnWarehouseAdded(ISLAND); }

    GameObjectId AddBuilding(uint16_t type) {
        GameObjectId id = objects.Create();
        objects.Building(id).type = type;
        objects.Building(id).island = ISLAND;
        objects.Anchor(id).footprint = glm::ivec2(3 * TILE_SIZE);
        return id;
    }

    void RunSeconds(int seconds) {
        for (int i = 0; i < seconds * NotificationSystem::CHECK_INTERVAL; i++) notifications.Update(objects, economy, ships, treasury, tick++);
    }

    std::vector<Notification> All() const {
        std::vector<Notification> all;
        notifications.ForEachSince(0, [&](const Notification& n) { all.push_back(n); });
        return all;
    }
};

} // namespace

TEST(NotificationTest, AProblemThatLastsIsToldOnce) {
    NotificationFixture f;
    GameObjectId sawmill = f.AddBuilding(BUILDING_SAWMILL);
    f.objects.Production(sawmill).status = ProducerStatus::MissingInput;
    f.RunSeconds(NotificationSystem::PROBLEM_SECONDS - 2);
    EXPECT_TRUE(f.All().empty()); // Not yet: it may pass
    f.RunSeconds(3);
    std::vector<Notification> all = f.All();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].kind, NotificationKind::MissingInput);
    EXPECT_EQ(all[0].param, (int)ItemType::Wood);
    EXPECT_EQ(all[0].building, sawmill);
    EXPECT_TRUE(all[0].located);
    f.RunSeconds(60);
    EXPECT_EQ(f.All().size(), 1u); // Not again within REPEAT_SECONDS
}

TEST(NotificationTest, TierReachedOnceAndOutOfCoins) {
    NotificationFixture f;
    f.economy.Find(ISLAND)->population[TIER_WORKERS] = 5;
    f.treasury.SetCoins(-10);
    f.RunSeconds(5);
    std::vector<Notification> all = f.All();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].kind, NotificationKind::TierReached);
    EXPECT_EQ(all[0].param, TIER_WORKERS);
    EXPECT_EQ(all[1].kind, NotificationKind::OutOfCoins);
    EXPECT_EQ(f.notifications.LastSequence(), 2u);
    uint32_t seen = 0;
    f.notifications.ForEachSince(1, [&](const Notification&) { seen++; });
    EXPECT_EQ(seen, 1u); // Only the newer one
}
