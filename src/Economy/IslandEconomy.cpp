#include "Economy/IslandEconomy.h"

#include <algorithm>

IslandEconomyManager::IslandEconomyManager() {
    m_Islands.reserve(MAX_ISLANDS);
}

IslandStorage* IslandEconomyManager::Find(IslandId island) {
    for (IslandStorage& storage : m_Islands) {
        if (storage.island == island) return &storage;
    }
    return nullptr;
}

const IslandStorage* IslandEconomyManager::Find(IslandId island) const {
    for (const IslandStorage& storage : m_Islands) {
        if (storage.island == island) return &storage;
    }
    return nullptr;
}

void IslandEconomyManager::OnWarehouseAdded(IslandId island) {
    if (island == NO_ISLAND) return;
    IslandStorage* storage = Find(island);
    if (!storage) {
        if (m_Islands.size() >= MAX_ISLANDS) return;
        m_Islands.push_back(IslandStorage()); // Within the reserved capacity
        storage = &m_Islands.back();
        storage->island = island;
        if (m_Islands.size() == 1) storage->amounts = STARTING_GOODS;
    }
    storage->warehouseCount++;
}

void IslandEconomyManager::OnWarehouseRemoved(IslandId island) {
    IslandStorage* storage = Find(island);
    if (!storage || storage->warehouseCount == 0) return;
    storage->warehouseCount--;
    for (int& amount : storage->amounts) amount = std::min(amount, storage->CapacityPerItem());
}

int IslandEconomyManager::Add(IslandId island, ItemType item, int amount) {
    IslandStorage* storage = Find(island);
    if (!storage || amount <= 0) return 0;
    int& stored = storage->amounts[(size_t)item];
    int added = std::clamp(storage->CapacityPerItem() - stored, 0, amount);
    stored += added;
    return added;
}

namespace {

// The change of a counter over the sampled time, scaled to a minute
int PerMinute(const IslandStorage& storage, int now, const std::array<int, ITEM_COUNT> IslandStorage::Sample::*field, size_t item) {
    if (storage.stockSamples < 2) return 0;
    int oldest = storage.stockSamples < TREND_SAMPLES ? 0 : storage.stockHead;
    int intervals = std::min(storage.stockSamples, TREND_SAMPLES) - 1;
    return (now - (storage.history[oldest].*field)[item]) * (TREND_SAMPLES - 1) / intervals;
}

} // namespace

int IslandStorage::TrendPerMinute(ItemType item) const {
    return PerMinute(*this, Amount(item), &Sample::stock, (size_t)item);
}

int IslandStorage::MadePerMinute(ItemType item) const {
    return PerMinute(*this, made[(size_t)item], &Sample::made, (size_t)item);
}

int IslandStorage::UsedPerMinute(ItemType item) const {
    return PerMinute(*this, used[(size_t)item], &Sample::used, (size_t)item);
}

void IslandEconomyManager::RecordTrends(uint64_t tick) {
    if (tick % TREND_INTERVAL_TICKS != 0) return;
    for (IslandStorage& storage : m_Islands) {
        IslandStorage::Sample sample{ storage.amounts, storage.made, storage.used };
        if (storage.stockSamples < TREND_SAMPLES) {
            storage.history[storage.stockSamples++] = sample;
        } else {
            storage.history[storage.stockHead] = sample; // Over the oldest
            storage.stockHead = (storage.stockHead + 1) % TREND_SAMPLES;
        }
    }
}

void IslandEconomyManager::AddAll(IslandId island, int amount) {
    for (int i = 0; i < ITEM_COUNT; i++) Add(island, (ItemType)i, amount);
}

int IslandEconomyManager::Remove(IslandId island, ItemType item, int amount) {
    IslandStorage* storage = Find(island);
    if (!storage || amount <= 0) return 0;
    int& stored = storage->amounts[(size_t)item];
    int removed = std::min(stored, amount);
    stored -= removed;
    return removed;
}
