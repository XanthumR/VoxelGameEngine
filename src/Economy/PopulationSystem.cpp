#include "Economy/PopulationSystem.h"

#include "Economy/IslandEconomy.h"
#include "Simulation/BuildingTypes.h"

#include <algorithm>
#include <cstdlib>

namespace {

// Moves a smoothed per-mille value a quarter of the way to the new sample (exactly onto it when
// close, so a steady supply reaches 1000)
int16_t Smooth(int16_t current, int sample) {
    int difference = sample - current;
    if (std::abs(difference) < 4) return (int16_t)sample;
    return (int16_t)(current + difference / 4);
}

} // namespace

PopulationSystem::PopulationSystem() {
    m_LookChanges.reserve(GameObjectRegistry::MAX_OBJECTS);
}

// Only within the reserved capacity (the frame loop clears the list every frame)
void PopulationSystem::PushLookChange(GameObjectId id) {
    if (m_LookChanges.size() < m_LookChanges.capacity()) m_LookChanges.push_back(id);
}

void PopulationSystem::Update(GameObjectRegistry& objects, IslandEconomyManager& economy, uint64_t tick) {
    if (tick % CONSUMPTION_INTERVAL == 0) Consume(objects, economy);
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || BUILDING_TYPES[objects.Building(id).type].role != BuildingRole::Residence) continue;
        UpdateHouse(objects, economy, id);
    }
}

void PopulationSystem::Consume(const GameObjectRegistry& objects, IslandEconomyManager& economy) {
    // Count residents per island and tier
    for (size_t i = 0; i < economy.IslandSlotCount(); i++) {
        IslandStorage& storage = economy.IslandAt(i);
        storage.population.fill(0);
        storage.suppliedResidents.fill(0);
    }
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingComponent& building = objects.Building(id);
        const BuildingType& type = BUILDING_TYPES[building.type];
        if (type.role != BuildingRole::Residence) continue;
        IslandStorage* storage = economy.Find(building.island);
        if (!storage) continue;
        int residents = objects.Residence(id).residents;
        storage->population[type.tier] += residents;
        const LogisticsComponent& logistics = objects.Logistics(id);
        if (logistics.connected && logistics.inMarketRange) storage->suppliedResidents[type.tier] += residents;
    }

    // Consume, tier by tier, so lower tiers get scarce goods first
    for (size_t i = 0; i < economy.IslandSlotCount(); i++) {
        IslandStorage& storage = economy.IslandAt(i);
        for (int tier = 0; tier < TIER_COUNT; tier++) {
            const PopulationTier& definition = POPULATION_TIERS[tier];
            for (int n = 0; n < definition.needCount; n++) {
                const Need& need = definition.needs[n];
                if (need.kind != NeedKind::Good) continue;
                int32_t& owed = storage.owed[tier][n];
                int32_t dueThisCycle = storage.suppliedResidents[tier] * need.consumption;
                // Owe at most one good beyond this cycle, so a long shortage is not paid back at once
                owed = std::min(owed + dueThisCycle, OWED_PER_GOOD + dueThisCycle);
                int wholeGoods = owed / OWED_PER_GOOD;
                int taken = economy.Remove(storage.island, need.item, wholeGoods);
                owed -= taken * OWED_PER_GOOD;

                // This cycle's supply: everything owed was delivered, or (nothing due yet) the good is in stock
                int sample;
                if (wholeGoods > 0) sample = taken * 1000 / wholeGoods;
                else sample = storage.Amount(need.item) > 0 ? 1000 : 0;
                storage.supply[tier][n] = Smooth(storage.supply[tier][n], sample);
            }
        }
    }
}

void PopulationSystem::UpdateHouse(GameObjectRegistry& objects, IslandEconomyManager& economy, GameObjectId id) {
    BuildingComponent& building = objects.Building(id);
    const BuildingType& type = BUILDING_TYPES[building.type];
    const PopulationTier& tier = POPULATION_TIERS[type.tier];
    ResidenceComponent& residence = objects.Residence(id);
    const LogisticsComponent& logistics = objects.Logistics(id);
    IslandStorage* storage = economy.Find(building.island);

    // Need supply of this house
    bool served = logistics.connected && logistics.inMarketRange && storage;
    bool allNeedsMet = true;
    for (int n = 0; n < MAX_NEEDS; n++) {
        int16_t supply = 0;
        if (n < tier.needCount && served) {
            supply = tier.needs[n].kind == NeedKind::Service ? (int16_t)1000 : storage->supply[type.tier][n];
        }
        residence.needSupply[n] = supply;
        if (n < tier.needCount && supply < UPGRADE_SUPPLY) allNeedsMet = false;
    }

    // Residents move in or out, one at a time
    int target = TargetResidents(tier, residence.needSupply);
    if (++residence.growthTicks >= GROWTH_INTERVAL) {
        residence.growthTicks = 0;
        if (residence.residents < target) residence.residents++;
        else if (residence.residents > target) residence.residents--;
    }

    // Upgrade to the next tier: full, every need met for a while, and planks in storage
    bool canRise = type.tier + 1 < TIER_COUNT && residence.residents >= tier.maxResidents && allNeedsMet;
    residence.upgradeTicks = canRise ? (uint16_t)std::min(residence.upgradeTicks + 1, UPGRADE_TICKS) : (uint16_t)0;
    if (residence.upgradeTicks >= UPGRADE_TICKS && storage && storage->Amount(ItemType::Planks) >= UPGRADE_PLANKS) {
        economy.Remove(building.island, ItemType::Planks, UPGRADE_PLANKS);
        building.type = RESIDENCE_FOR_TIER[type.tier + 1];
        residence.upgradeTicks = 0;
        residence.downgradeTicks = 0;
        PushLookChange(id);
        m_Upgrades++;
        return;
    }

    // Downgrade: no more residents than the tier below holds, for a long time
    if (type.tier > 0) {
        const PopulationTier& below = POPULATION_TIERS[type.tier - 1];
        bool falling = residence.residents <= below.maxResidents;
        residence.downgradeTicks = falling ? (uint16_t)std::min(residence.downgradeTicks + 1, DOWNGRADE_TICKS) : (uint16_t)0;
        if (residence.downgradeTicks >= DOWNGRADE_TICKS) {
            building.type = RESIDENCE_FOR_TIER[type.tier - 1];
            residence.residents = (uint8_t)std::min<int>(residence.residents, below.maxResidents);
            residence.downgradeTicks = 0;
            residence.upgradeTicks = 0;
            PushLookChange(id);
            m_Downgrades++;
        }
    }
}
