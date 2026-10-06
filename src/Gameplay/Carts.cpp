#include "Gameplay/Carts.h"

#include "Economy/ProductionChains.h"
#include "Gameplay/FigureModels.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Placement.h"

#include <algorithm>
#include <cmath>

namespace {

int DirectionIndex(glm::ivec2 step) {
    if (step.x > 0) return 0;
    if (step.x < 0) return 1;
    return step.y > 0 ? 2 : 3;
}

// The right-hand side of driving along step (the same frame as figures.comp)
glm::vec2 RightOf(glm::ivec2 step) {
    return glm::vec2(-step.y, step.x);
}

glm::vec2 TileMiddle(glm::ivec2 tile) {
    return glm::vec2(tile * TILE_SIZE) + glm::vec2((TILE_SIZE - 1) * 0.5f);
}

} // namespace

CartPlacement PlaceCart(const ProductionComponent& production, float alpha) {
    CartPlacement placement;
    if (production.cartState == CartState::Idle || production.cartPathLength == 0) return placement;
    placement.visible = true;

    const int last = production.cartPathLength - 1;
    const bool outbound = production.cartState == CartState::ToWarehouse || production.cartState == CartState::Unloading;
    placement.moving = production.cartState == CartState::ToWarehouse || production.cartState == CartState::ToProducer;

    // Position between ticks: a moving cart is ahead of its last tick by alpha of a tick's drive
    float position = (float)production.cartPosition;
    if (placement.moving) position += (outbound ? 1.0f : -1.0f) * CART_SPEED * std::clamp(alpha, 0.0f, 1.0f);
    position = std::clamp(position, 0.0f, (float)(last * CART_TILE));
    placement.travelled = position / CART_TILE * TILE_SIZE;

    if (last == 0) {
        // A route of one tile: stands in its middle, facing along the road the way it found it
        placement.column = TileMiddle(production.cartPath[0]);
        return placement;
    }

    // The segment from path[segment] to path[segment + 1], and how far along it (toward the warehouse)
    int segment = std::min((int)(position / CART_TILE), last - 1);
    float along = position / CART_TILE - (float)segment;
    glm::ivec2 from = production.cartPath[segment], to = production.cartPath[segment + 1];
    glm::ivec2 step = outbound ? to - from : from - to;

    // The lane moves over to the new side through the first half of a segment after a turn
    glm::ivec2 previousStep = step;
    float into = outbound ? along : 1.0f - along;
    if (outbound && segment > 0) previousStep = from - production.cartPath[segment - 1];
    if (!outbound && segment + 2 <= last) previousStep = production.cartPath[segment + 1] - production.cartPath[segment + 2];
    float turn = std::min(1.0f, into * 2.0f);
    glm::vec2 right = glm::mix(RightOf(previousStep), RightOf(step), turn);
    glm::vec2 facing = glm::mix(glm::vec2(previousStep), glm::vec2(step), turn);

    placement.column = glm::mix(TileMiddle(from), TileMiddle(to), along) + right * CART_LANE_OFFSET;
    placement.direction = DirectionIndex(step);
    placement.yaw = std::atan2(facing.x, facing.y);
    // At the warehouse it turns around on the spot while unloading, ready to drive back
    if (production.cartState == CartState::Unloading) {
        placement.yaw += 3.14159265f * std::min(1.0f, (float)production.cartWaitTicks / CART_UNLOAD_TICKS);
    }
    return placement;
}

void AppendCartObjects(const GameObjectRegistry& objects, float alpha, int modelBase, std::vector<VoxelObject>& out) {
    int drawn = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount() && drawn < MAX_CARTS; slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
        if (type.role != BuildingRole::Producer) continue;
        const ProductionComponent& production = objects.Production(id);
        CartPlacement placement = PlaceCart(production, alpha);
        if (!placement.visible) continue;

        // The cargo: the output on the way out, the first input it brings on the way back
        const ProductionChain& chain = PRODUCTION_CHAINS[type.chain];
        int item = (int)chain.output, amount = production.cartOutput;
        if (production.cartState == CartState::ToProducer && amount == 0) {
            for (int i = 0; i < chain.inputCount; i++) {
                if (production.cartInputs[i] == 0) continue;
                item = (int)chain.inputs[i];
                amount = production.cartInputs[i];
                break;
            }
        }

        int frame = placement.moving ? (int)(placement.travelled / CART_TROT_STRIDE) & 3 : 1;
        VoxelObject object;
        object.model = modelBase + CartModelOffset(frame, item, std::min(amount, CART_CAPACITY));
        object.position = glm::vec3(placement.column.x + 0.5f, (float)BUILD_GROUND_Y, placement.column.y + 0.5f);
        object.yaw = placement.yaw;
        out.push_back(object); // Within the caller's reserve
        drawn++;
    }
}
