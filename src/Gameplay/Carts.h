#pragma once

#include "Gameplay/Figure.h"
#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <vector>

// Where a producer's cart is drawn: the simulation moves carts along their road tiles at the
// fixed tick (ProductionSystem); this places them between ticks and in the right-hand lane.
struct CartPlacement {
    bool visible = false;   // False while the cart is at home
    glm::vec2 column{ 0 };  // Voxel column of the cart's center
    int direction = 0;      // Facing (0 +x, 1 -x, 2 +z, 3 -z)
    bool moving = false;
    float travelled = 0.0f; // Voxels along the route, drives the trot and the wheels
};

constexpr int MAX_CARTS = 128;            // Carts drawn at once
constexpr float CART_LANE_OFFSET = 2.5f;  // Voxels right of the road's middle
constexpr float CART_TROT_STRIDE = 2.0f;  // Voxels per frame of the trot

// alpha: how far the clock is into the next tick (GameClock::Alpha), 0..1
CartPlacement PlaceCart(const ProductionComponent& production, float alpha);

// The figures of every cart on the road, up to MAX_CARTS
void AppendCartFigures(const GameObjectRegistry& objects, float alpha, std::vector<Figure>& out);
