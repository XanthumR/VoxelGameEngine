#pragma once

#include "Economy/IslandEconomy.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Ships.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

class Treasury;

// What the player is told about, as in Anno 1800's notification feed
enum class NotificationKind : uint8_t {
    NoWorkers,        // A producer has had no workers for a while
    MissingInput,     // ... or has been waiting for an input (param: the good)
    HouseDowngrading, // A house is about to fall a tier
    UpgradeReady,     // Houses on an island are ready to upgrade
    StorageFull,      // An island's storage of a good is full (param: the good)
    TierReached,      // The first residents of a tier moved in on an island (param: the tier)
    OutOfCoins,       // The treasury is below zero
    ShipDocked,       // A ship that is on no route docked at a harbor
};

struct Notification {
    NotificationKind kind = NotificationKind::NoWorkers;
    IslandId island = NO_ISLAND;
    GameObjectId building = INVALID_GAME_OBJECT; // The building it is about, if any
    uint16_t buildingType = 0xFFFF;              // Its type (it may be gone by the time it is read)
    ShipId ship = INVALID_SHIP;
    glm::ivec2 column{ 0 };  // Where to look (world columns); for OutOfCoins nowhere in particular
    bool located = false;    // column is meaningful
    int param = 0;           // A good (ItemType) or a tier, by kind
    uint64_t tick = 0;
    uint32_t sequence = 0;   // 1, 2, ... in the order they happened
};

// Watches the simulation once a second and keeps the last MAX_KEPT notifications. A building's
// problem must last PROBLEM_SECONDS before it is told, and the same building is told about again
// at most every REPEAT_SECONDS; island-wide ones (storage full, houses ready) are rate-limited per
// island too. Deterministic, allocated up front.
class NotificationSystem {
public:
    static constexpr int CHECK_INTERVAL = 10;   // Ticks between checks (1 s)
    static constexpr int MAX_KEPT = 32;
    static constexpr int PROBLEM_SECONDS = 20;
    static constexpr int REPEAT_SECONDS = 180;
    static constexpr int ISLAND_REPEAT_SECONDS = 300;

    NotificationSystem();

    void Update(const GameObjectRegistry& objects, const IslandEconomyManager& economy, const ShipSystem& ships, const Treasury& treasury,
        uint64_t tick);

    uint32_t LastSequence() const { return m_Sequence; }
    // The kept notifications newer than sequence `after`, oldest first
    template <typename Function>
    void ForEachSince(uint32_t after, Function function) const {
        uint32_t first = m_Sequence > (uint32_t)MAX_KEPT ? m_Sequence - MAX_KEPT + 1 : 1;
        for (uint32_t sequence = std::max(first, after + 1); sequence <= m_Sequence; sequence++) function(m_Kept[sequence % MAX_KEPT]);
    }

private:
    void Push(Notification notification, uint64_t tick);
    // Where an island-wide notification points: the island's first storage building
    void LocateIsland(const GameObjectRegistry& objects, Notification& notification) const;

    std::array<Notification, MAX_KEPT> m_Kept{};
    uint32_t m_Sequence = 0;

    // Per object slot: the problem it has, for how many seconds, and when it was last told about
    std::vector<uint8_t> m_ProblemKind; // NotificationKind + 1, 0 = none
    std::vector<uint16_t> m_ProblemSeconds;
    std::vector<int64_t> m_LastTold; // Starts long ago, so the first problem is told

    // Per island (IslandEconomyManager's slot order): the highest tier reached, when its storage of
    // each good was last told full, when its upgrades were last told
    std::vector<int8_t> m_TierReached;
    std::vector<std::array<int64_t, ITEM_COUNT>> m_FullTold;
    std::vector<int64_t> m_UpgradeTold;
    int64_t m_CoinsTold = -1000000;
    std::array<ShipState, ShipSystem::MAX_SHIPS> m_ShipStates{};
};
