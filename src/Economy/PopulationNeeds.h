#pragma once

#include "Simulation/ItemType.h"

#include <array>
#include <cstdint>

// What each population tier needs, how many residents each need lets move in, and how fast goods
// are consumed. Tiers are processed in this order (Farmers first get scarce goods first).

// Public buildings (BuildingRole::Service) whose reach along the roads meets a need
enum class ServiceType : uint8_t {
    Marketplace, // Also brings the goods: a house outside every marketplace's reach gets none
    School,
    Theatre,
    Count
};

constexpr int SERVICE_COUNT = (int)ServiceType::Count;
constexpr std::array<const char*, SERVICE_COUNT> SERVICE_NAMES = { "Marketplace", "School", "Variety Theatre" };
constexpr std::array<int, SERVICE_COUNT> SERVICE_ROAD_RANGE = { 20, 16, 24 }; // Road tiles each type reaches

enum class NeedKind : uint8_t {
    Service, // Met by being in a service building's reach
    Good,    // Consumed from island storage
};

struct Need {
    NeedKind kind = NeedKind::Good;
    ItemType item = ItemType::Wood;                 // Good needs
    ServiceType service = ServiceType::Marketplace; // Service needs
    uint8_t residentsGranted = 0;                   // Residents this need allows when fully supplied
    uint16_t consumption = 0;                       // Good needs: thousandths of a good per resident per minute
};

constexpr Need GoodNeed(ItemType item, int residents, int consumption) {
    return { NeedKind::Good, item, ServiceType::Marketplace, (uint8_t)residents, (uint16_t)consumption };
}
constexpr Need ServiceNeed(ServiceType service, int residents) {
    return { NeedKind::Service, ItemType::Wood, service, (uint8_t)residents, 0 };
}
constexpr const char* NeedName(const Need& need) {
    return need.kind == NeedKind::Good ? ItemName(need.item) : SERVICE_NAMES[(size_t)need.service];
}

constexpr int MAX_NEEDS = 8;
constexpr int TIER_COUNT = 3;
constexpr int TIER_FARMERS = 0;
constexpr int TIER_WORKERS = 1;
constexpr int TIER_ARTISANS = 2;

struct PopulationTier {
    const char* name;
    uint8_t maxResidents; // Per house
    uint8_t needCount;
    std::array<Need, MAX_NEEDS> needs;
};

constexpr std::array<PopulationTier, TIER_COUNT> POPULATION_TIERS = { {
    { "Farmers", 10, 3, { {
        ServiceNeed(ServiceType::Marketplace, 2),
        GoodNeed(ItemType::Fish, 4, 50),
        GoodNeed(ItemType::WorkClothes, 4, 40),
    } } },
    { "Workers", 20, 7, { {
        ServiceNeed(ServiceType::Marketplace, 2),
        GoodNeed(ItemType::Fish, 3, 50),
        GoodNeed(ItemType::WorkClothes, 3, 40),
        GoodNeed(ItemType::Sausages, 3, 30),
        GoodNeed(ItemType::Bread, 3, 40),
        GoodNeed(ItemType::Soap, 3, 25),
        ServiceNeed(ServiceType::School, 3),
    } } },
    { "Artisans", 30, 8, { {
        ServiceNeed(ServiceType::Marketplace, 3),
        ServiceNeed(ServiceType::School, 3),
        GoodNeed(ItemType::Sausages, 3, 30),
        GoodNeed(ItemType::Bread, 3, 40),
        GoodNeed(ItemType::Soap, 3, 25),
        GoodNeed(ItemType::CannedFood, 5, 25),
        GoodNeed(ItemType::SewingMachines, 5, 15),
        ServiceNeed(ServiceType::Theatre, 5),
    } } },
} };

// Sum of the residents each need grants
constexpr int GrantedResidents(const PopulationTier& tier) {
    int total = 0;
    for (int i = 0; i < tier.needCount; i++) total += tier.needs[i].residentsGranted;
    return total;
}
constexpr bool NeedsFillHouses() {
    for (const PopulationTier& tier : POPULATION_TIERS) {
        if (GrantedResidents(tier) != tier.maxResidents) return false;
    }
    return true;
}
static_assert(NeedsFillHouses(), "Each tier's needs must add up to a full house");

// Residents a house of this tier can hold with these need supplies (per mille, one per need)
int TargetResidents(const PopulationTier& tier, const std::array<int16_t, MAX_NEEDS>& needSupply);
