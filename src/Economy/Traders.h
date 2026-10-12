#pragma once

#include "Simulation/GameObjects.h"
#include "Simulation/ItemType.h"

#include <array>
#include <cstdint>

class SaveReader;
class SaveWriter;
class Treasury;

// What a good is worth in coins: a little more than its making costs (the upkeep of its whole
// chain per good, Economy/Balance.h), so prices follow the economy when it is tuned. A trader pays
// the value for it, so selling a surplus earns a little; they ask half as much again for it.
int GoodValue(ItemType item);
inline int SellPrice(ItemType item) { return GoodValue(item); }                     // The player gets, per good
inline int BuyPrice(ItemType item) { return (GoodValue(item) * 3 + 1) / 2; }        // The player pays, per good

// A trader of the sea, as in Anno 1800: one per session, with a trading post on an island of their
// own (World/Sessions.h). They sell a few wares from a stock that fills up again over time, and buy
// any good at its value, as much as the player brings.
struct TraderType {
    static constexpr int MAX_WARES = 8;
    const char* name;
    int session;
    int wareCount;
    std::array<ItemType, MAX_WARES> wares;
};

constexpr std::array<TraderType, 2> TRADERS = { {
    { "Captain Elias Marlowe", 0, 8, { ItemType::Planks, ItemType::Bricks, ItemType::SteelBeams, ItemType::Windows, ItemType::Fish,
        ItemType::WorkClothes, ItemType::Soap, ItemType::Coal } },
    { "Rosalia Vega", 1, 7, { ItemType::Planks, ItemType::Bricks, ItemType::Fish, ItemType::Bread, ItemType::Sausages, ItemType::Copper,
        ItemType::Gold } },
} };
constexpr int TRADER_COUNT = (int)TRADERS.size();

class TraderSystem {
public:
    static constexpr int MAX_STOCK = 50;     // Of each ware
    static constexpr int RESTOCK_TICKS = 50; // One more of each ware this often (5 s)

    TraderSystem();
    void Update(uint64_t tick);

    // The trading post of each trader, once it stands (INVALID_GAME_OBJECT before)
    void SetPost(int trader, GameObjectId post) { m_Posts[trader] = post; }
    GameObjectId Post(int trader) const { return m_Posts[trader]; }
    int TraderAt(GameObjectId post) const; // -1 when it is no trading post

    bool Sells(int trader, ItemType item) const;
    int Stock(int trader, ItemType item) const { return m_Stock[trader][(size_t)item]; }
    // Buys up to amount of a ware, as many as the stock and the coins allow; returns how many
    int Buy(int trader, ItemType item, int amount, Treasury& treasury);
    // Sells amount of a good to the trader (they take any amount); returns how many
    int Sell(int trader, ItemType item, int amount, Treasury& treasury);

    void Save(SaveWriter& out) const;
    void Load(SaveReader& in);

private:
    std::array<GameObjectId, TRADER_COUNT> m_Posts;
    std::array<std::array<int16_t, ITEM_COUNT>, TRADER_COUNT> m_Stock{};
};
