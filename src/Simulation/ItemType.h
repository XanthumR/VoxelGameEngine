#pragma once

#include <array>
#include <cstdint>

// Every good that can be stored, produced or consumed. The order is the storage index.
enum class ItemType : uint8_t {
    Wood,
    Planks,
    Fish,
    Wool,
    WorkClothes,
    Bricks,
    Count
};

constexpr int ITEM_COUNT = (int)ItemType::Count;

constexpr std::array<const char*, ITEM_COUNT> ITEM_NAMES = { "Wood", "Planks", "Fish", "Wool", "Work Clothes", "Bricks" };

constexpr const char* ItemName(ItemType item) {
    return ITEM_NAMES[(size_t)item];
}
