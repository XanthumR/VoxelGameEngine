#include "Economy/Traders.h"

#include "Economy/Balance.h"
#include "Economy/Treasury.h"
#include "Simulation/SaveGame.h"

#include <algorithm>
#include <cmath>

int GoodValue(ItemType item) {
    static const std::array<int, ITEM_COUNT> values = [] {
        std::array<int, ITEM_COUNT> table{};
        for (int i = 0; i < ITEM_COUNT; i++) table[i] = std::max(1, (int)std::lround(ChainUpkeepPerGood((ItemType)i) * 1.1));
        return table;
    }();
    return values[(size_t)item];
}

TraderSystem::TraderSystem() {
    m_Posts.fill(INVALID_GAME_OBJECT);
    for (int trader = 0; trader < TRADER_COUNT; trader++) {
        for (int w = 0; w < TRADERS[trader].wareCount; w++) m_Stock[trader][(size_t)TRADERS[trader].wares[w]] = MAX_STOCK;
    }
}

void TraderSystem::Update(uint64_t tick) {
    if (tick % RESTOCK_TICKS != 0) return;
    for (int trader = 0; trader < TRADER_COUNT; trader++) {
        for (int w = 0; w < TRADERS[trader].wareCount; w++) {
            int16_t& stock = m_Stock[trader][(size_t)TRADERS[trader].wares[w]];
            stock = (int16_t)std::min(MAX_STOCK, stock + 1);
        }
    }
}

int TraderSystem::TraderAt(GameObjectId post) const {
    if (post == INVALID_GAME_OBJECT) return -1;
    for (int trader = 0; trader < TRADER_COUNT; trader++) {
        if (m_Posts[trader] == post) return trader;
    }
    return -1;
}

bool TraderSystem::Sells(int trader, ItemType item) const {
    const TraderType& type = TRADERS[trader];
    return std::find(type.wares.begin(), type.wares.begin() + type.wareCount, item) != type.wares.begin() + type.wareCount;
}

int TraderSystem::Buy(int trader, ItemType item, int amount, Treasury& treasury) {
    if (!Sells(trader, item) || amount <= 0) return 0;
    int16_t& stock = m_Stock[trader][(size_t)item];
    int price = BuyPrice(item);
    int bought = (int)std::min<int64_t>({ (int64_t)amount, (int64_t)stock, std::max<int64_t>(0, treasury.Coins()) / price });
    stock = (int16_t)(stock - bought);
    treasury.SetCoins(treasury.Coins() - (int64_t)bought * price);
    return bought;
}

int TraderSystem::Sell(int, ItemType item, int amount, Treasury& treasury) {
    if (amount <= 0) return 0;
    treasury.SetCoins(treasury.Coins() + (int64_t)amount * SellPrice(item));
    return amount;
}

void TraderSystem::Save(SaveWriter& out) const {
    out.Write(m_Posts);
    out.Write(m_Stock);
}

void TraderSystem::Load(SaveReader& in) {
    in.Read(m_Posts);
    in.Read(m_Stock);
}
