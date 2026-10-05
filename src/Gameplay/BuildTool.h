#pragma once

#include "Gameplay/Picking.h"
#include "Rendering/BuildPreview.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Placement.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

struct GLFWwindow;
class RoadTool;
class Simulation;
class VoxelWorld;
class WorldEditor;

// Placing and demolishing buildings from the strategy camera, and the build selection (a building
// type, the road tool, or nothing). With a building type selected the footprint follows the
// cursor, snapped to the tile grid; R rotates it and left click places it when the preview is
// green. With nothing selected, right click demolishes the building or road tile under the cursor.
class BuildTool {
public:
    static constexpr int NO_TYPE = -1;
    static constexpr int ROAD = -2; // The road tool is selected (RoadTool handles the mouse)

    // Build menu order: the buildable types (hotkeys 1, 2, ...), then the road (the next number)
    static int BuildableCount();
    static int BuildableAt(int index); // Building type of the index-th buildable type

    BuildTool(const VoxelWorld& world, WorldEditor& editor, Simulation& simulation, RoadTool& roads);

    void SelectType(int type);
    int SelectedType() const { return m_SelectedType; }
    uint8_t Rotation() const { return m_Rotation; }

    // Once per frame in strategy mode. mouseFree / keyboardFree: the UI does not want the input.
    void Update(GLFWwindow* window, const PickResult& hover, bool mouseFree, bool keyboardFree);

    // For the renderer and the UI; valid after Update
    const BuildPreview& Preview() const { return m_Preview; }
    bool HasPlacementPreview() const { return m_HasPlacement; }
    glm::ivec2 PreviewMinTile() const { return m_PreviewMinTile; }
    glm::ivec2 PreviewTiles() const { return m_PreviewTiles; }
    PlacementError LastError() const { return m_LastCheck.error; }
    bool PreviewConnected() const { return m_PreviewConnected; } // The previewed footprint touches road in range
    bool PreviewInMarketRange() const { return m_PreviewInMarket; } // ... and a marketplace's reach
    GameObjectId HoveredBuilding() const { return m_HoveredBuilding; }

    GameObjectId Place(uint16_t type, uint8_t rotation, glm::ivec2 minTile); // INVALID if not placeable
    void Demolish(GameObjectId id);

    // Rebuilds the voxels of a building whose type changed in place (house upgrades)
    void RefreshLook(GameObjectId id);

private:
    const VoxelWorld& m_World;
    WorldEditor& m_Editor;
    Simulation& m_Simulation;
    RoadTool& m_RoadTool;

    int m_SelectedType = NO_TYPE;
    uint8_t m_Rotation = 0;
    PlacementCheck m_LastCheck;
    GameObjectId m_HoveredBuilding = INVALID_GAME_OBJECT;
    BuildPreview m_Preview;
    bool m_HasPlacement = false;
    glm::ivec2 m_PreviewMinTile = glm::ivec2(0);
    glm::ivec2 m_PreviewTiles = glm::ivec2(0);
    bool m_PreviewConnected = false;
    bool m_PreviewInMarket = false;
    std::vector<uint8_t> m_LookBuffer; // Reused for every placement

    bool m_LeftWasPressed = false, m_RightWasPressed = false, m_RWasPressed = false;
    std::array<bool, 9> m_NumberWasPressed = {}; // Keys 1-9
};
