// Writes the economy's numbers (Economy/Balance.h) as Markdown, for docs/BALANCE.md:
//   build/bin/Release/BalanceReport > docs/BALANCE.md
#include "Economy/Balance.h"
#include "Economy/Traders.h"
#include "Economy/Treasury.h"
#include "Simulation/BuildingTypes.h"

#include <cstdio>

namespace {

const char* ChainName(int chain) {
    for (const BuildingType& type : BUILDING_TYPES) {
        if (type.chain == chain && type.role == BuildingRole::Producer) return type.name;
    }
    return "?";
}

} // namespace

int main() {
    constexpr int RESIDENTS = 100;
    std::printf("# Balance\n\n");
    std::printf("Worked out from the game's tables by `tools/balance/BalanceReport.cpp` (`Economy/Balance.h`); written again after "
                "every change to the numbers. Everything is per %d residents of a tier with every need met and every producer at "
                "full productivity; producers are counted in fractions and followed down to their raw goods. Service buildings are "
                "counted by the houses one of them serves on average (road range, a house per 15 tiles).\n\n", RESIDENTS);

    std::printf("## Summary\n\n| Tier | Taxes / min | Producers' upkeep | Services' upkeep | Net / min | Jobs by tier (F, W, A, E, I) |\n");
    std::printf("|---|---:|---:|---:|---:|---|\n");
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        TierBalance b = BalanceFor(tier, RESIDENTS);
        std::printf("| %s | %.0f | %.0f | %.0f | **%+.0f** | %.0f, %.0f, %.0f, %.0f, %.0f |\n", POPULATION_TIERS[tier].name, b.taxes,
            b.producerUpkeep, b.serviceUpkeep, b.Net(), b.workers[0], b.workers[1], b.workers[2], b.workers[3], b.workers[4]);
    }

    for (int tier = 0; tier < TIER_COUNT; tier++) {
        TierBalance b = BalanceFor(tier, RESIDENTS);
        const PopulationTier& population = POPULATION_TIERS[tier];
        std::printf("\n## %s (%d per house, %d houses)\n\n", population.name, population.maxResidents, RESIDENTS / population.maxResidents);
        std::printf("| Need | Per minute |\n|---|---:|\n");
        for (int n = 0; n < population.needCount; n++) {
            const Need& need = population.needs[n];
            if (need.kind == NeedKind::Good) std::printf("| %s | %.1f |\n", NeedName(need), RESIDENTS * need.consumption / 1000.0);
            else std::printf("| %s | %.2f buildings |\n", NeedName(need), b.services[(size_t)need.service]);
        }
        std::printf("\n| Producer | Count | Jobs | Upkeep / min |\n|---|---:|---:|---:|\n");
        for (int chain = 0; chain < (int)PRODUCTION_CHAINS.size(); chain++) {
            if (b.producers[chain] <= 0.0) continue;
            int type = 0;
            while (BUILDING_TYPES[type].chain != chain || BUILDING_TYPES[type].role != BuildingRole::Producer) type++;
            const ProductionChain& recipe = PRODUCTION_CHAINS[chain];
            std::printf("| %s | %.2f | %.1f %s | %.0f |\n", ChainName(chain), b.producers[chain], b.producers[chain] * recipe.workforce,
                POPULATION_TIERS[recipe.workforceTier].name, b.producers[chain] * BUILDING_COSTS[type].upkeep);
        }
    }

    std::printf("\n## Producers\n\nA good's chain upkeep is what its producer and every producer feeding it cost per good made. A "
                "trader pays the good's value (and asks half as much again), so making goods to sell only pays where the value is "
                "higher.\n\n| Producer | Makes / min | Workers | Upkeep / min | Chain upkeep per good | Trader pays |\n");
    std::printf("|---|---:|---:|---:|---:|---:|\n");
    for (int chain = 0; chain < (int)PRODUCTION_CHAINS.size(); chain++) {
        int type = 0;
        while (BUILDING_TYPES[type].chain != chain || BUILDING_TYPES[type].role != BuildingRole::Producer) type++;
        const ProductionChain& recipe = PRODUCTION_CHAINS[chain];
        double rate = OutputPerMinute(chain);
        std::printf("| %s | %.1f %s | %d %s | %d | %.1f | %d |\n", ChainName(chain), rate, ItemName(recipe.output), recipe.workforce,
            POPULATION_TIERS[recipe.workforceTier].name, BUILDING_COSTS[type].upkeep, ChainUpkeepPerGood(recipe.output),
            SellPrice(recipe.output));
    }
    return 0;
}
