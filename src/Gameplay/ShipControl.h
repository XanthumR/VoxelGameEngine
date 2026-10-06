#pragma once

#include "Gameplay/Picking.h"
#include "Rendering/VoxelObject.h"
#include "Simulation/Ships.h"

#include <array>
#include <vector>

struct GLFWwindow;
class Simulation;

// The player's hand on the ships, and their look. With nothing to build selected, a left click
// near a ship selects it (elsewhere deselects); with a ship selected, a right click on the sea
// sends it there and a right click on a harbor sends it to the harbor's berth. Ships are voxel
// objects: a two-masted trade ship that turns smoothly toward where it sails and rides the ocean's waves.
class ShipControl {
public:
    static constexpr float SELECT_RADIUS = 1.6f; // Tiles from the clicked point to a ship's middle
    static constexpr float TURN_RATE = 1.2f;     // Radians per second a ship turns

    // The trade ship with its sails furled or set, registered with the renderer
    static VoxelObjectModel BuildModel(bool sailsSet);
    void SetModels(int anchored, int sailing) { m_AnchoredModel = anchored; m_SailingModel = sailing; }

    // mouseFree: the cursor is not over the UI; building: a building or road is selected to place
    void Update(GLFWwindow* window, const PickResult& hover, bool mouseFree, bool building, Simulation& simulation);
    void AppendObjects(const ShipSystem& ships, float alpha, float deltaTime, std::vector<VoxelObject>& out);

    ShipId Selected() const { return m_Selected; }
    void Deselect() { m_Selected = INVALID_SHIP; }
    bool LastOrderFailed() const { return m_OrderFailed; }

private:
    ShipId m_Selected = INVALID_SHIP;
    bool m_OrderFailed = false;
    bool m_LeftWasDown = false, m_RightWasDown = false;
    int m_AnchoredModel = 0, m_SailingModel = 0;
    std::array<float, ShipSystem::MAX_SHIPS> m_Yaw{};
    std::array<ShipId, ShipSystem::MAX_SHIPS> m_YawOf{}; // The ship each yaw belongs to
};
