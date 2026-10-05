#include "Economy/PopulationNeeds.h"

int TargetResidents(const PopulationTier& tier, const std::array<int16_t, MAX_NEEDS>& needSupply) {
    int target = 0;
    for (int i = 0; i < tier.needCount; i++) target += tier.needs[i].residentsGranted * needSupply[i] / 1000;
    return target;
}
