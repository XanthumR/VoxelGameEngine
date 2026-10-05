#pragma once

#include "Simulation/ItemType.h"

#include <array>
#include <cstdint>

// What each population tier needs, how many residents each need lets move in, and how fast goods
// are consumed. Tiers are processed in this order (Farmers first get scarce goods first).

enum class NeedKind : uint8_t {
    Service, // Met by being in a building's reach (the marketplace)
    Good,    // Consumed from island storage
};

struct Need {
    const char* name;
    NeedKind kind;
    ItemType item;            // Good needs only
    uint8_t residentsGranted; // Residents this need allows when fully supplied
    uint16_t consumption;     // Good needs: thousandths of a good per resident per minute
};

constexpr int MAX_NEEDS = 4;
constexpr int TIER_COUNT = 2;
constexpr int TIER_FARMERS = 0;
constexpr int TIER_WORKERS = 1;

struct PopulationTier {
    const char* name;
    uint8_t maxResidents; // Per house
    uint8_t needCount;
    std::array<Need, MAX_NEEDS> needs;
};

constexpr std::array<PopulationTier, TIER_COUNT> POPULATION_TIERS = { {
    { "Farmers", 10, 3, { {
        { "Marketplace", NeedKind::Service, ItemType::Wood, 2, 0 },
        { "Fish", NeedKind::Good, ItemType::Fish, 4, 50 },
        { "Work Clothes", NeedKind::Good, ItemType::WorkClothes, 4, 40 },
        {},
    } } },
    { "Workers", 20, 4, { {
        { "Marketplace", NeedKind::Service, ItemType::Wood, 2, 0 },
        { "Fish", NeedKind::Good, ItemType::Fish, 6, 50 },
        { "Work Clothes", NeedKind::Good, ItemType::WorkClothes, 6, 40 },
        { "Sausages", NeedKind::Good, ItemType::Sausages, 6, 30 },
    } } },
} };

constexpr int UPGRADE_PLANKS = 2; // Planks a house upgrade takes from island storage

// Sum of the residents each need grants
constexpr int GrantedResidents(const PopulationTier& tier) {
    int total = 0;
    for (int i = 0; i < tier.needCount; i++) total += tier.needs[i].residentsGranted;
    return total;
}
static_assert(GrantedResidents(POPULATION_TIERS[TIER_FARMERS]) == POPULATION_TIERS[TIER_FARMERS].maxResidents, "Farmer needs must add up to a full house");
static_assert(GrantedResidents(POPULATION_TIERS[TIER_WORKERS]) == POPULATION_TIERS[TIER_WORKERS].maxResidents, "Worker needs must add up to a full house");

// Residents a house of this tier can hold with these need supplies (per mille, one per need)
int TargetResidents(const PopulationTier& tier, const std::array<int16_t, MAX_NEEDS>& needSupply);
