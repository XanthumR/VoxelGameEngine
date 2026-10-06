#include "Economy/Treasury.h"

#include "Economy/IslandEconomy.h"

#include <algorithm>

int64_t Treasury::HouseTaxMilli(const GameObjectRegistry& objects, GameObjectId id) {
    const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
    const PopulationTier& tier = POPULATION_TIERS[type.tier];
    const ResidenceComponent& residence = objects.Residence(id);
    int64_t supply = 0;
    for (int n = 0; n < tier.needCount; n++) supply += std::clamp<int>(residence.needSupply[n], 0, 1000);
    supply /= std::max<int>(1, tier.needCount);
    // tax per ten residents x residents / 10 x supply / 1000, in thousandths of a coin
    return (int64_t)TAX_PER_TEN_RESIDENTS[type.tier] * residence.residents * supply / 10;
}

void Treasury::Update(const GameObjectRegistry& objects) {
    m_IncomeMilli = 0;
    m_UpkeepMilli = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        uint16_t type = objects.Building(id).type;
        if (BUILDING_TYPES[type].role == BuildingRole::Residence) m_IncomeMilli += HouseTaxMilli(objects, id);
        m_UpkeepMilli += BUILDING_COSTS[type].upkeep * 1000;
    }

    // A tick is 1/600 of a minute: whole coins move once the remainder holds them
    m_Fraction += m_IncomeMilli - m_UpkeepMilli;
    const int64_t coin = 1000 * TICKS_PER_MINUTE;
    m_Coins += m_Fraction / coin;
    m_Fraction %= coin;
}

PlacementError Treasury::Check(uint16_t type, IslandId island, const IslandEconomyManager& economy) const {
    const BuildingCost& cost = BUILDING_COSTS[type];
    if (m_Coins < cost.coins) return PlacementError::NotEnoughCoins;
    if (cost.planks > 0) {
        const IslandStorage* storage = economy.Find(island);
        if (!storage || storage->Amount(ItemType::Planks) < cost.planks) return PlacementError::NotEnoughPlanks;
    }
    return PlacementError::None;
}

void Treasury::Pay(uint16_t type, IslandId island, IslandEconomyManager& economy) {
    const BuildingCost& cost = BUILDING_COSTS[type];
    m_Coins -= cost.coins;
    if (cost.planks > 0) economy.Remove(island, ItemType::Planks, cost.planks);
}

void Treasury::Refund(uint16_t type, IslandId island, IslandEconomyManager& economy) {
    const BuildingCost& cost = BUILDING_COSTS[type];
    m_Coins += cost.coins / 2;
    if (cost.planks / 2 > 0) economy.Add(island, ItemType::Planks, cost.planks / 2);
}
