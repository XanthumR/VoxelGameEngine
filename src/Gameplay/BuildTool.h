#pragma once

#include "Gameplay/Picking.h"
#include "Rendering/BuildPreview.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Placement.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

struct GLFWwindow;
class Simulation;
class VoxelWorld;
class WorldEditor;

// Placing and demolishing buildings from the strategy camera. With a building type selected the
// footprint follows the cursor, snapped to the tile grid; R rotates it and left click places it
// when the preview is green. Right click demolishes the building under the cursor, or drops the
// selected type when there is none.
class BuildTool {
public:
    static constexpr int NO_TYPE = -1;

    BuildTool(const VoxelWorld& world, WorldEditor& editor, Simulation& simulation);

    void SelectType(int type) { m_SelectedType = type; }
    int SelectedType() const { return m_SelectedType; }
    uint8_t Rotation() const { return m_Rotation; }

    // Once per frame in strategy mode. mouseFree / keyboardFree: the UI does not want the input.
    void Update(GLFWwindow* window, const PickResult& hover, bool mouseFree, bool keyboardFree);

    // For the renderer and the overlay; valid after Update
    const BuildPreview& Preview() const { return m_Preview; }
    PlacementError LastError() const { return m_LastCheck.error; }
    GameObjectId HoveredBuilding() const { return m_HoveredBuilding; }

    GameObjectId Place(uint16_t type, uint8_t rotation, glm::ivec2 minTile); // INVALID if not placeable
    void Demolish(GameObjectId id);

private:
    const VoxelWorld& m_World;
    WorldEditor& m_Editor;
    Simulation& m_Simulation;

    int m_SelectedType = NO_TYPE;
    uint8_t m_Rotation = 0;
    PlacementCheck m_LastCheck;
    GameObjectId m_HoveredBuilding = INVALID_GAME_OBJECT;
    BuildPreview m_Preview;
    std::vector<uint8_t> m_LookBuffer; // Reused for every placement

    bool m_LeftWasPressed = false, m_RightWasPressed = false, m_RWasPressed = false;
    bool m_Key1WasPressed = false, m_Key2WasPressed = false;
};
