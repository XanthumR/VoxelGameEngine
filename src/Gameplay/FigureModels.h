#pragma once

#include "Economy/PopulationNeeds.h"
#include "Rendering/VoxelObject.h"
#include "Simulation/ItemType.h"

// Voxel object models of the walkers and the carts, one per look and animation frame (a voxel model
// cannot bend, so the walk and trot cycles are separate models). The renderer gets them in this
// order, so a model is found by its offset from the first one.

// People: per tier, 8 looks (skin, shirt, trousers) and 4 walk frames
constexpr int PERSON_TIERS = TIER_COUNT, PERSON_VARIANTS = 8, WALK_FRAMES = 4;
constexpr int PERSON_MODEL_COUNT = PERSON_TIERS * PERSON_VARIANTS * WALK_FRAMES;
constexpr int PersonModelOffset(int tier, int variant, int frame) { return (tier * PERSON_VARIANTS + variant) * WALK_FRAMES + frame; }
VoxelObjectModel BuildPersonModel(int tier, int variant, int frame);

// Carts: 4 trot frames, each empty or carrying 1-4 rows of crates or of a loose pile. The cargo
// voxels are Block::CARGO, drawn as the good's block (VoxelObject::cargo), so one model serves
// every good.
constexpr int CART_FRAMES = 4, CART_LOADS = 1 + 2 * 4;
constexpr int CART_MODEL_COUNT = CART_FRAMES * CART_LOADS;
constexpr bool CargoIsPiled(ItemType item) {
    return item == ItemType::Wood || item == ItemType::Pigs || item == ItemType::Clay || item == ItemType::Coal || item == ItemType::Iron;
}
constexpr int CartModelOffset(int frame, bool piled, int amount) { return frame * CART_LOADS + (amount <= 0 ? 0 : 1 + (piled ? 4 : 0) + (amount - 1)); }
VoxelObjectModel BuildCartModel(int frame, bool piled, int amount);
