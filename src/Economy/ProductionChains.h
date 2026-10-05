#pragma once

#include "Economy/PopulationNeeds.h"
#include "Simulation/ItemType.h"

#include <array>
#include <cstdint>

// The production buildings' recipes: who works there, what goes in and out, how long a cycle
// takes, and where the building may stand. Index = BuildingType::chain.

// Where a producer may stand and what makes it faster (src/Simulation/ProducerLocation.h)
enum class LocationRule : uint8_t {
    None,
    Coast,   // Required: open sea within radius tiles of the footprint
    Trees,   // Productivity: standing trees within radius tiles, full speed at fullSpeedCount
    Pasture, // Productivity: free grass tiles within radius tiles, full speed at fullSpeedCount
};

struct ProductionChain {
    uint8_t workforceTier;   // POPULATION_TIERS index the workers come from
    uint8_t workforce;       // Workers needed for full speed
    uint8_t inputCount;      // 0, 1 or 2
    std::array<ItemType, 2> inputs; // One of each per cycle
    ItemType output;         // One per cycle
    uint16_t cycleTicks;     // At full productivity (10 ticks per second)
    LocationRule rule;
    uint8_t radius;          // Tiles around the footprint the rule looks at
    uint8_t fullSpeedCount;  // Trees or pasture tiles for full productivity
};

constexpr std::array<ProductionChain, 7> PRODUCTION_CHAINS = { {
    { TIER_FARMERS, 5, 0, { ItemType::Wood, ItemType::Wood }, ItemType::Fish, 300, LocationRule::Coast, 2, 0 },              // Fishery
    { TIER_FARMERS, 5, 0, { ItemType::Wood, ItemType::Wood }, ItemType::Wood, 200, LocationRule::Trees, 6, 6 },              // Lumberjack
    { TIER_FARMERS, 5, 1, { ItemType::Wood, ItemType::Wood }, ItemType::Planks, 200, LocationRule::None, 0, 0 },             // Sawmill
    { TIER_FARMERS, 5, 0, { ItemType::Wood, ItemType::Wood }, ItemType::Wool, 300, LocationRule::Pasture, 3, 24 },           // Sheep Farm
    { TIER_FARMERS, 10, 1, { ItemType::Wool, ItemType::Wool }, ItemType::WorkClothes, 300, LocationRule::None, 0, 0 },       // Framework Knitter
    { TIER_WORKERS, 10, 0, { ItemType::Wood, ItemType::Wood }, ItemType::Pigs, 600, LocationRule::Pasture, 3, 24 },          // Pig Farm
    { TIER_WORKERS, 15, 1, { ItemType::Pigs, ItemType::Pigs }, ItemType::Sausages, 300, LocationRule::None, 0, 0 },          // Slaughterhouse
} };

constexpr int PRODUCER_BUFFER = 4; // Goods a producer holds of its output and of each input
constexpr int CART_CAPACITY = 4;   // Goods a cart carries per trip
