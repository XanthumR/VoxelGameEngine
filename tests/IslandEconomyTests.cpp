#include "Economy/IslandEconomy.h"

#include <gtest/gtest.h>

TEST(IslandEconomyTest, UnsettledIslandHasNoStorage) {
    IslandEconomyManager economy;
    EXPECT_EQ(economy.Find(3), nullptr);
    EXPECT_EQ(economy.Add(3, ItemType::Wood, 10), 0);
    EXPECT_EQ(economy.Remove(3, ItemType::Wood, 10), 0);
}

TEST(IslandEconomyTest, FirstWarehouseSeedsStartingGoods) {
    IslandEconomyManager economy;
    economy.OnWarehouseAdded(3);
    const IslandStorage* storage = economy.Find(3);
    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->warehouseCount, 1);
    EXPECT_EQ(storage->CapacityPerItem(), WAREHOUSE_CAPACITY);
    EXPECT_EQ(storage->Amount(ItemType::Planks), IslandEconomyManager::STARTING_GOODS[(size_t)ItemType::Planks]);

    // A second warehouse adds capacity but no more goods
    economy.Remove(3, ItemType::Planks, 1000);
    economy.OnWarehouseAdded(3);
    EXPECT_EQ(storage->warehouseCount, 2);
    EXPECT_EQ(storage->CapacityPerItem(), 2 * WAREHOUSE_CAPACITY);
    EXPECT_EQ(storage->Amount(ItemType::Planks), 0);
}

TEST(IslandEconomyTest, IslandsHaveSeparateStorage) {
    IslandEconomyManager economy;
    economy.OnWarehouseAdded(1);
    economy.OnWarehouseAdded(2);
    economy.Add(1, ItemType::Fish, 5);
    EXPECT_EQ(economy.Find(1)->Amount(ItemType::Fish), 5);
    EXPECT_EQ(economy.Find(2)->Amount(ItemType::Fish), 0);
    EXPECT_EQ(economy.SettledIslandCount(), 2u);
}

TEST(IslandEconomyTest, AddStopsAtCapacityAndRemoveAtZero) {
    IslandEconomyManager economy;
    economy.OnWarehouseAdded(1);
    EXPECT_EQ(economy.Add(1, ItemType::Wool, WAREHOUSE_CAPACITY + 7), WAREHOUSE_CAPACITY);
    EXPECT_EQ(economy.Add(1, ItemType::Wool, 1), 0);
    EXPECT_EQ(economy.Remove(1, ItemType::Wool, 20), 20);
    EXPECT_EQ(economy.Remove(1, ItemType::Wool, 1000), WAREHOUSE_CAPACITY - 20);
    EXPECT_EQ(economy.Find(1)->Amount(ItemType::Wool), 0);
    EXPECT_EQ(economy.Add(1, ItemType::Wool, -5), 0);
}

TEST(IslandEconomyTest, RemovingAWarehouseShrinksCapacity) {
    IslandEconomyManager economy;
    economy.OnWarehouseAdded(1);
    economy.OnWarehouseAdded(1);
    economy.Add(1, ItemType::Bricks, 80);
    economy.OnWarehouseRemoved(1);
    EXPECT_EQ(economy.Find(1)->CapacityPerItem(), WAREHOUSE_CAPACITY);
    EXPECT_EQ(economy.Find(1)->Amount(ItemType::Bricks), WAREHOUSE_CAPACITY); // Overflow is lost
    economy.OnWarehouseRemoved(1);
    economy.OnWarehouseRemoved(1); // More removals than warehouses are ignored
    EXPECT_EQ(economy.Find(1)->warehouseCount, 0);
}

TEST(IslandEconomyTest, NoIslandIsIgnored) {
    IslandEconomyManager economy;
    economy.OnWarehouseAdded(NO_ISLAND);
    EXPECT_EQ(economy.SettledIslandCount(), 0u);
}
