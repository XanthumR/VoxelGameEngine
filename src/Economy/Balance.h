#pragma once

#include "Economy/PopulationNeeds.h"
#include "Economy/ProductionChains.h"

#include <array>

// The economy's numbers worked out from the tables, for balancing (tools/balance writes them to
// docs/BALANCE.md): what keeps a number of residents of one tier fully supplied, at full
// productivity, and what they pay. Producers are counted in fractions (2.5 fisheries); the chains
// are followed down to their raw goods (work clothes need wool, so sheep farms too). Service
// buildings are counted by how many houses one of them serves on average (SERVICE_HOUSES).
struct TierBalance {
    int tier = 0;
    int residents = 0;
    std::array<double, PRODUCTION_CHAINS.size()> producers{}; // Per chain
    std::array<double, TIER_COUNT> workers{};                 // Jobs in those producers, per tier
    std::array<double, SERVICE_COUNT> services{};             // Service buildings
    double taxes = 0.0;           // Coins per minute, every need met
    double producerUpkeep = 0.0;  // Coins per minute
    double serviceUpkeep = 0.0;
    double Net() const { return taxes - producerUpkeep - serviceUpkeep; }
};

// Houses one service building of each type serves, on average: its road range in tiles, a diamond
// of about 2 x range^2 tiles reached, one house per 15 tiles (3 x 3 tiles, its share of the road
// and of the producers between the houses)
constexpr std::array<double, SERVICE_COUNT> ServiceHouses() {
    std::array<double, SERVICE_COUNT> houses{};
    for (int i = 0; i < SERVICE_COUNT; i++) houses[i] = 2.0 * SERVICE_ROAD_RANGE[i] * SERVICE_ROAD_RANGE[i] / 15.0;
    return houses;
}
constexpr std::array<double, SERVICE_COUNT> SERVICE_HOUSES = ServiceHouses();

// Goods made per minute by one producer of a chain at full productivity
constexpr double OutputPerMinute(int chain) { return 600.0 / PRODUCTION_CHAINS[chain].cycleTicks; }

// The producer chain that makes a good, -1 for none
int ChainMaking(ItemType item);

TierBalance BalanceFor(int tier, int residents);

// The upkeep of the whole chain behind one good a minute (its producer and every one feeding it),
// per good: what making it costs, to set against what a trader pays
double ChainUpkeepPerGood(ItemType item);
