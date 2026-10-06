#pragma once

#include "Rendering/VoxelObject.h"
#include "Simulation/ItemType.h"

// Voxel object models of the walkers and the carts, one per look and animation frame (a voxel model
// cannot bend, so the walk and trot cycles are separate models). The renderer gets them in this
// order, so a model is found by its offset from the first one.

// People: per tier (Farmers, Workers), 8 looks (skin, shirt, trousers) and 4 walk frames
constexpr int PERSON_TIERS = 2, PERSON_VARIANTS = 8, WALK_FRAMES = 4;
constexpr int PERSON_MODEL_COUNT = PERSON_TIERS * PERSON_VARIANTS * WALK_FRAMES;
constexpr int PersonModelOffset(int tier, int variant, int frame) { return (tier * PERSON_VARIANTS + variant) * WALK_FRAMES + frame; }
VoxelObjectModel BuildPersonModel(int tier, int variant, int frame);

// Carts: 4 trot frames, each empty or carrying 1-4 of any good
constexpr int CART_FRAMES = 4, CART_LOADS = 1 + ITEM_COUNT * 4;
constexpr int CART_MODEL_COUNT = CART_FRAMES * CART_LOADS;
constexpr int CartModelOffset(int frame, int item, int amount) { return frame * CART_LOADS + (amount <= 0 ? 0 : 1 + item * 4 + (amount - 1)); }
VoxelObjectModel BuildCartModel(int frame, int item, int amount);
