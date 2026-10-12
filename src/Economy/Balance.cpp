#include "Economy/Balance.h"

#include "Economy/Treasury.h"
#include "Simulation/BuildingTypes.h"

namespace {

// The building type of a chain (its upkeep)
int TypeOfChain(int chain) {
    for (int type = 0; type < (int)BUILDING_TYPES.size(); type++) {
        if (BUILDING_TYPES[type].chain == chain && BUILDING_TYPES[type].role == BuildingRole::Producer) return type;
    }
    return -1;
}

int TypeOfService(ServiceType service) {
    for (int type = 0; type < (int)BUILDING_TYPES.size(); type++) {
        if (BUILDING_TYPES[type].role == BuildingRole::Service && BUILDING_TYPES[type].service == service) return type;
    }
    return -1;
}

// perMinute of a good, and everything that goes into it
void Supply(TierBalance& balance, ItemType item, double perMinute) {
    int chain = ChainMaking(item);
    if (chain < 0 || perMinute <= 0.0) return;
    const ProductionChain& recipe = PRODUCTION_CHAINS[chain];
    double producers = perMinute / OutputPerMinute(chain);
    balance.producers[chain] += producers;
    balance.workers[recipe.workforceTier] += producers * recipe.workforce;
    for (int i = 0; i < recipe.inputCount; i++) Supply(balance, recipe.inputs[i], perMinute); // One of each per good made
}

} // namespace

int ChainMaking(ItemType item) {
    for (int chain = 0; chain < (int)PRODUCTION_CHAINS.size(); chain++) {
        if (PRODUCTION_CHAINS[chain].output == item) return chain;
    }
    return -1;
}

double ChainUpkeepPerGood(ItemType item) {
    TierBalance balance;
    Supply(balance, item, 1.0);
    double upkeep = 0.0;
    for (int chain = 0; chain < (int)PRODUCTION_CHAINS.size(); chain++) {
        int type = TypeOfChain(chain);
        if (type >= 0) upkeep += balance.producers[chain] * BUILDING_COSTS[type].upkeep;
    }
    return upkeep;
}

TierBalance BalanceFor(int tier, int residents) {
    TierBalance balance;
    balance.tier = tier;
    balance.residents = residents;
    const PopulationTier& population = POPULATION_TIERS[tier];
    double houses = (double)residents / population.maxResidents;
    for (int n = 0; n < population.needCount; n++) {
        const Need& need = population.needs[n];
        if (need.kind == NeedKind::Good) Supply(balance, need.item, residents * need.consumption / 1000.0);
        else balance.services[(size_t)need.service] += houses / SERVICE_HOUSES[(size_t)need.service];
    }
    balance.taxes = residents * TAX_PER_TEN_RESIDENTS[tier] / 10.0;
    for (int chain = 0; chain < (int)PRODUCTION_CHAINS.size(); chain++) {
        int type = TypeOfChain(chain);
        if (type >= 0) balance.producerUpkeep += balance.producers[chain] * BUILDING_COSTS[type].upkeep;
    }
    for (int s = 0; s < SERVICE_COUNT; s++) {
        int type = TypeOfService((ServiceType)s);
        if (type >= 0) balance.serviceUpkeep += balance.services[s] * BUILDING_COSTS[type].upkeep;
    }
    return balance;
}
