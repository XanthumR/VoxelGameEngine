#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Every good that can be stored, produced or consumed. The order is the storage index.
enum class ItemType : uint8_t {
    Wood,
    Planks,
    Fish,
    Wool,
    WorkClothes,
    Bricks,
    Sausages,
    Pigs,
    Grain,
    Flour,
    Bread,
    Tallow,
    Soap,
    Clay,
    Beef,
    Iron,
    Coal,
    Steel,
    SteelBeams,
    CannedFood,
    SewingMachines,
    Count
};

constexpr int ITEM_COUNT = (int)ItemType::Count;

constexpr std::array<const char*, ITEM_COUNT> ITEM_NAMES = { "Wood", "Planks", "Fish", "Wool", "Work Clothes", "Bricks", "Sausages", "Pigs", "Grain",
    "Flour", "Bread", "Tallow", "Soap", "Clay", "Beef", "Iron", "Coal", "Steel", "Steel Beams", "Canned Food", "Sewing Machines" };

constexpr const char* ItemName(ItemType item) {
    return ITEM_NAMES[(std::size_t)item];
}
