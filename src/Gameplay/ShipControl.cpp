#include "Gameplay/ShipControl.h"

#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"
#include "World/BlockTypes.h"
#include "World/WorldConstants.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

// Model size: 11 wide, 24 tall, 31 long; the waterline is 3 voxels above the bottom
constexpr glm::ivec3 SHIP_SIZE(11, 24, 31);
constexpr float WATERLINE = 3.0f;

} // namespace

VoxelObjectModel ShipControl::BuildModel(bool sailsSet) {
    VoxelObjectModel model;
    model.size = SHIP_SIZE;
    model.ids.assign((size_t)SHIP_SIZE.x * SHIP_SIZE.y * SHIP_SIZE.z, 0);
    auto put = [&](int s, int z, int y, uint8_t id) { // s: -5..5 across, z: 0 (stern) .. 30 (bow)
        int x = s + SHIP_SIZE.x / 2;
        if (x < 0 || x >= SHIP_SIZE.x || z < 0 || z >= SHIP_SIZE.z || y < 0 || y >= SHIP_SIZE.y) return;
        model.ids[(size_t)x + (size_t)SHIP_SIZE.x * ((size_t)z + (size_t)SHIP_SIZE.z * (size_t)y)] = id;
    };
    auto halfWidth = [](int z) {
        if (z >= 30) return 0;
        if (z >= 28) return 1;
        if (z >= 26) return 2;
        if (z >= 24) return 3;
        if (z >= 21 || z <= 3) return 4;
        return 5;
    };

    // Hull: dark below the waterline, a blue stripe, wooden sides with a gilded rail, a plank deck
    for (int z = 0; z < SHIP_SIZE.z; z++) {
        int w = halfWidth(z);
        for (int s = -w; s <= w; s++) {
            int a = std::abs(s);
            if (a <= std::max(w - 3, 0)) put(s, z, 0, Block::TIMBER_DARK);
            if (a <= std::max(w - 2, 0)) put(s, z, 1, Block::TIMBER_DARK);
            if (a <= std::max(w - 1, 0)) put(s, z, 2, Block::TIMBER_DARK);
            if (a == w) {
                put(s, z, 3, Block::SHUTTER_BLUE);
                put(s, z, 4, Block::BOAT_WOOD);
                put(s, z, 5, Block::BOAT_WOOD);
                put(s, z, 6, Block::AWNING_YELLOW);
            } else {
                put(s, z, 3, Block::TIMBER_DARK);
                put(s, z, 4, Block::TIMBER_LIGHT); // Deck
            }
        }
    }
    for (int y = 4; y <= 8; y++) put(0, 30, y, Block::BOAT_WOOD); // Stem post

    // Stern castle with windows aft and a rail on top
    for (int z = 1; z <= 6; z++) {
        int w = halfWidth(z);
        for (int s = -w; s <= w; s++) {
            for (int y = 5; y <= 8; y++) {
                if (std::abs(s) == w || z == 1 || z == 6) put(s, z, y, Block::BOAT_WOOD);
            }
            put(s, z, 8, Block::TIMBER_LIGHT);
            if (std::abs(s) == w || z == 1) put(s, z, 9, Block::AWNING_YELLOW);
        }
    }
    for (int s = -2; s <= 2; s += 2) put(s, 1, 6, Block::WINDOW_GLASS);
    put(0, 1, 9, Block::IRON); // Stern lantern
    put(0, 1, 10, Block::WINDOW_GLASS);

    // Cargo lashed on deck
    for (int z = 10; z <= 13; z++) {
        for (int s = -2; s <= 2; s++) put(s, z, 5, Block::CRATE);
        for (int s = -1; s <= 1; s++) put(s, z, 6, Block::CRATE);
    }
    put(-3, 15, 5, Block::BARREL);
    put(3, 15, 5, Block::BARREL);

    // Main and fore masts with yards; square sails hang behind the masts when set, furled otherwise
    struct Mast {
        int z, top, yard, sailLow, half;
    };
    for (const Mast& mast : { Mast{ 17, 22, 20, 10, 5 }, Mast{ 24, 19, 17, 9, 3 } }) {
        for (int y = 5; y <= mast.top; y++) put(0, mast.z, y, Block::TIMBER_DARK);
        for (int s = -mast.half; s <= mast.half; s++) put(s, mast.z, mast.yard, Block::TIMBER_LIGHT);
        if (sailsSet) {
            for (int y = mast.sailLow; y < mast.yard; y++) {
                for (int s = -mast.half + 1; s <= mast.half - 1; s++) put(s, mast.z - 1, y, Block::AWNING_WHITE);
            }
        } else {
            for (int s = -mast.half + 1; s <= mast.half - 1; s++) put(s, mast.z - 1, mast.yard - 1, Block::AWNING_WHITE);
        }
    }
    put(1, 17, 23, Block::AWNING_RED); // Pennant on the main mast
    put(2, 17, 23, Block::AWNING_RED);
    put(1, 17, 22, Block::AWNING_RED);
    return model;
}

void ShipControl::Update(GLFWwindow* window, const PickResult& hover, bool mouseFree, bool building, Simulation& simulation) {
    bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool leftClick = left && !m_LeftWasDown, rightClick = right && !m_RightWasDown;
    m_LeftWasDown = left;
    m_RightWasDown = right;

    ShipSystem& ships = simulation.Ships();
    if (building || !ships.IsAlive(m_Selected)) m_Selected = INVALID_SHIP;
    if (building || !mouseFree || !hover.hit) return;

    glm::vec2 clicked = glm::vec2(hover.voxel.x, hover.voxel.z) / (float)TILE_SIZE;
    if (leftClick) {
        m_Selected = ships.Nearest(clicked, SELECT_RADIUS);
        m_OrderFailed = false;
    }
    if (rightClick && m_Selected != INVALID_SHIP) {
        glm::ivec2 tile(ColumnToTile(hover.voxel.x), ColumnToTile(hover.voxel.z));
        GameObjectId occupant = simulation.Occupancy().At(tile);
        bool harbor = occupant != INVALID_GAME_OBJECT && BUILDING_TYPES[simulation.Objects().Building(occupant).type].dockRows > 0 &&
                      BUILDING_TYPES[simulation.Objects().Building(occupant).type].role == BuildingRole::Storage;
        m_OrderFailed = !ships.SailTo(m_Selected, tile, simulation.Objects(), simulation.Occupancy(), harbor ? occupant : INVALID_GAME_OBJECT);
    }
}

void ShipControl::AppendObjects(const ShipSystem& ships, float alpha, float deltaTime, std::vector<VoxelObject>& out) {
    for (int slot = 0; slot < ShipSystem::MAX_SHIPS; slot++) {
        ShipId id = ships.IdAtSlot(slot);
        if (id == INVALID_SHIP) continue;
        const Ship& ship = ships.Get(id);
        glm::vec2 velocity = ship.position - ship.previous;

        // Turn toward the way it sails (a new ship starts facing it)
        if (glm::length(velocity) > 1e-5f) {
            float target = std::atan2(velocity.x, velocity.y);
            if (m_YawOf[slot] != id) {
                m_Yaw[slot] = target;
            } else {
                float delta = std::remainder(target - m_Yaw[slot], 6.28318531f);
                m_Yaw[slot] += std::clamp(delta, -TURN_RATE * deltaTime, TURN_RATE * deltaTime);
            }
        }
        m_YawOf[slot] = id;

        glm::vec2 at = glm::mix(ship.previous, ship.position, std::clamp(alpha, 0.0f, 1.0f)) * (float)TILE_SIZE;
        // It rides the waves (the GPU sets its height, pitch and roll from the ocean)
        VoxelObject object;
        object.model = ship.state == ShipState::Sailing ? m_SailingModel : m_AnchoredModel;
        object.position = glm::vec3(at.x, SEA_LEVEL + 1.0f - WATERLINE, at.y);
        object.yaw = m_Yaw[slot];
        object.waterline = WATERLINE;
        out.push_back(object); // Within the caller's reserve
    }
}
