#include "Economy/Balance.h"
#include "Economy/Traders.h"

#include <gtest/gtest.h>

// The economy's shape (docs/BALANCE.md): every tier pays more in taxes than what supplies it
// costs, needs fewer jobs of its own tier than it has residents to spare, and trading never beats
// making a good by much
TEST(BalanceTest, EveryTierPaysItsWayAndCanStaffItsProducers) {
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        TierBalance balance = BalanceFor(tier, 100);
        EXPECT_GT(balance.Net(), 0.2 * balance.taxes) << POPULATION_TIERS[tier].name;
        EXPECT_LT(balance.workers[tier], 50.0) << POPULATION_TIERS[tier].name;
    }
}

TEST(BalanceTest, TradersPriceGoodsAboveWhatMakingThemCosts) {
    for (int i = 0; i < ITEM_COUNT; i++) {
        double cost = ChainUpkeepPerGood((ItemType)i);
        EXPECT_GE(SellPrice((ItemType)i), cost) << ItemName((ItemType)i);        // Selling a surplus does not lose
        EXPECT_LE(SellPrice((ItemType)i), cost * 1.25 + 1.0) << ItemName((ItemType)i); // ...and is no money machine
        EXPECT_GT(BuyPrice((ItemType)i), SellPrice((ItemType)i)) << ItemName((ItemType)i);
    }
}
