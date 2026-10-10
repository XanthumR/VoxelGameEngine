#include "Simulation/Notifications.h"

#include "Economy/PopulationSystem.h"
#include "Economy/ProductionChains.h"
#include "Economy/Treasury.h"
#include "Simulation/BuildingTypes.h"

namespace {

constexpr int64_t LONG_AGO = -1000000;

glm::ivec2 MiddleOf(const VoxelAnchorComponent& anchor) {
    return glm::ivec2(anchor.origin.x, anchor.origin.z) + anchor.footprint / 2;
}

} // namespace

NotificationSystem::NotificationSystem() {
    m_ProblemKind.assign(GameObjectRegistry::MAX_OBJECTS, 0);
    m_ProblemSeconds.assign(GameObjectRegistry::MAX_OBJECTS, 0);
    m_LastTold.assign(GameObjectRegistry::MAX_OBJECTS, LONG_AGO);
    m_TierReached.assign(IslandEconomyManager::MAX_ISLANDS, 0);
    std::array<int64_t, ITEM_COUNT> never;
    never.fill(LONG_AGO);
    m_FullTold.assign(IslandEconomyManager::MAX_ISLANDS, never);
    m_UpgradeTold.assign(IslandEconomyManager::MAX_ISLANDS, LONG_AGO);
    m_ShipStates.fill(ShipState::Idle);
}

void NotificationSystem::Push(Notification notification, uint64_t tick) {
    notification.tick = tick;
    notification.sequence = ++m_Sequence;
    m_Kept[m_Sequence % MAX_KEPT] = notification;
}

void NotificationSystem::LocateIsland(const GameObjectRegistry& objects, Notification& notification) const {
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || objects.Building(id).island != notification.island) continue;
        if (BUILDING_TYPES[objects.Building(id).type].role != BuildingRole::Storage) continue;
        notification.building = id;
        notification.buildingType = objects.Building(id).type;
        notification.column = MiddleOf(objects.Anchor(id));
        notification.located = true;
        return;
    }
}

void NotificationSystem::Update(const GameObjectRegistry& objects, const IslandEconomyManager& economy, const ShipSystem& ships,
    const Treasury& treasury, uint64_t tick) {
    if (tick % CHECK_INTERVAL != 0) return;
    const int64_t now = (int64_t)tick;
    const int64_t second = CHECK_INTERVAL; // Ticks per second: a check is every second

    // Buildings with a problem that lasts: producers without workers or an input, houses about to fall
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
        int kind = 0, param = 0;
        if (type.role == BuildingRole::Producer) {
            const ProductionComponent& production = objects.Production(id);
            if (production.status == ProducerStatus::NoWorkforce) kind = (int)NotificationKind::NoWorkers + 1;
            if (production.status == ProducerStatus::MissingInput) {
                kind = (int)NotificationKind::MissingInput + 1;
                const ProductionChain& chain = PRODUCTION_CHAINS[type.chain];
                for (int i = 0; i < chain.inputCount; i++) {
                    if (production.inputs[i] == 0) {
                        param = (int)chain.inputs[i];
                        break;
                    }
                }
            }
        } else if (type.role == BuildingRole::Residence && type.tier > 0) {
            if (objects.Residence(id).downgradeTicks >= PopulationSystem::DOWNGRADE_TICKS / 2) kind = (int)NotificationKind::HouseDowngrading + 1;
        }
        if (kind != m_ProblemKind[slot]) {
            m_ProblemKind[slot] = (uint8_t)kind;
            m_ProblemSeconds[slot] = 0;
        }
        if (kind == 0) continue;
        if (m_ProblemSeconds[slot] < 0xFFFF) m_ProblemSeconds[slot]++;
        bool lasted = m_ProblemSeconds[slot] >= PROBLEM_SECONDS || kind == (int)NotificationKind::HouseDowngrading + 1;
        if (!lasted || now - m_LastTold[slot] < REPEAT_SECONDS * second) continue;
        m_LastTold[slot] = now;
        Notification notification;
        notification.kind = (NotificationKind)(kind - 1);
        notification.island = objects.Building(id).island;
        notification.building = id;
        notification.buildingType = objects.Building(id).type;
        notification.column = MiddleOf(objects.Anchor(id));
        notification.located = true;
        notification.param = param;
        Push(notification, tick);
    }

    // Islands: a tier reached, a good's storage full, houses ready to upgrade
    for (size_t i = 0; i < economy.IslandSlotCount() && i < IslandEconomyManager::MAX_ISLANDS; i++) {
        const IslandStorage& storage = economy.IslandAt(i);
        for (int tier = m_TierReached[i] + 1; tier < TIER_COUNT; tier++) {
            if (storage.population[tier] == 0) break;
            m_TierReached[i] = (int8_t)tier;
            Notification notification;
            notification.kind = NotificationKind::TierReached;
            notification.island = storage.island;
            notification.param = tier;
            LocateIsland(objects, notification);
            Push(notification, tick);
        }
        int capacity = storage.CapacityPerItem();
        for (int item = 0; item < ITEM_COUNT && capacity > 0; item++) {
            if (storage.amounts[item] < capacity || now - m_FullTold[i][item] < ISLAND_REPEAT_SECONDS * second) continue;
            m_FullTold[i][item] = now;
            Notification notification;
            notification.kind = NotificationKind::StorageFull;
            notification.island = storage.island;
            notification.param = item;
            LocateIsland(objects, notification);
            Push(notification, tick);
        }
    }
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || !PopulationSystem::IsReadyToUpgrade(objects, id)) continue;
        IslandId island = objects.Building(id).island;
        for (size_t i = 0; i < economy.IslandSlotCount() && i < IslandEconomyManager::MAX_ISLANDS; i++) {
            if (economy.IslandAt(i).island != island || now - m_UpgradeTold[i] < ISLAND_REPEAT_SECONDS * second) continue;
            m_UpgradeTold[i] = now;
            Notification notification;
            notification.kind = NotificationKind::UpgradeReady;
            notification.island = island;
            notification.building = id;
            notification.buildingType = objects.Building(id).type;
            notification.column = MiddleOf(objects.Anchor(id));
            notification.located = true;
            Push(notification, tick);
        }
    }

    // The treasury
    if (treasury.Coins() < 0 && now - m_CoinsTold >= ISLAND_REPEAT_SECONDS * second) {
        m_CoinsTold = now;
        Notification notification;
        notification.kind = NotificationKind::OutOfCoins;
        Push(notification, tick);
    }

    // Ships on no route that just docked
    for (int slot = 0; slot < ShipSystem::MAX_SHIPS; slot++) {
        ShipId id = ships.IdAtSlot(slot);
        ShipState state = id == INVALID_SHIP ? ShipState::Idle : ships.Get(id).state;
        bool docked = state == ShipState::Docked && m_ShipStates[slot] != ShipState::Docked;
        m_ShipStates[slot] = state;
        if (!docked || ships.Get(id).route >= 0 || !objects.IsAlive(ships.Get(id).harbor)) continue;
        Notification notification;
        notification.kind = NotificationKind::ShipDocked;
        notification.ship = id;
        notification.building = ships.Get(id).harbor;
        notification.buildingType = objects.Building(notification.building).type;
        notification.island = objects.Building(notification.building).island;
        notification.column = glm::ivec2(ships.Get(id).position * (float)TILE_SIZE);
        notification.located = true;
        Push(notification, tick);
    }
}
