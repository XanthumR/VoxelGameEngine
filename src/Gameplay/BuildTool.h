#pragma once

#include "Gameplay/Picking.h"
#include "Rendering/BuildPreview.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Placement.h"
#include "Simulation/ProducerLocation.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

struct GLFWwindow;
class BuildingModelLibrary;
class RoadTool;
class TerrainGenerator;
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

    // Build menu tabs: the buildable types of a category (hotkeys 1, 2, ...); the Infrastructure
    // tab ends with the road
    static int EntryCount(BuildCategory tab);
    static int EntryAt(BuildCategory tab, int index); // Building type, or ROAD

    BuildCategory Tab() const { return m_Tab; }
    void SetTab(BuildCategory tab) { m_Tab = tab; }

    BuildTool(const VoxelWorld& world, WorldEditor& editor, Simulation& simulation, RoadTool& roads, const BuildingModelLibrary& models,
        TerrainGenerator& terrain);

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

    // The building clicked with nothing selected (its panel is open); INVALID when none
    GameObjectId InspectedBuilding() const { return m_InspectedBuilding; }
    void ClearInspection() { m_InspectedBuilding = INVALID_GAME_OBJECT; }

    // Producers: how the previewed spot does on the chain's location rule, and the tiles that count
    bool HasLocationPreview() const { return m_HasLocation; }
    const LocationReport& PreviewLocation() const { return m_Location; }
    const std::vector<glm::ivec2>& PreviewLocationTiles() const { return m_LocationTiles; }
    uint32_t LocationRevision() const { return m_LocationRevision; } // Changes when the above do

    GameObjectId Place(uint16_t type, uint8_t rotation, glm::ivec2 minTile); // INVALID if not placeable
    void Demolish(GameObjectId id);

    // Rebuilds the voxels of a building whose type changed in place (house upgrades)
    void RefreshLook(GameObjectId id);

private:
    // Puts the generated terrain back in a box (under a demolished dock)
    void RestoreTerrain(glm::ivec3 minCorner, glm::ivec3 size);

    const VoxelWorld& m_World;
    WorldEditor& m_Editor;
    Simulation& m_Simulation;
    RoadTool& m_RoadTool;
    const BuildingModelLibrary& m_Models;
    TerrainGenerator& m_Terrain;
    std::vector<uint8_t> m_ChunkScratch;   // One generated chunk (RestoreTerrain)
    std::vector<uint8_t> m_RestoreBuffer;  // The restored box

    int m_SelectedType = NO_TYPE;
    BuildCategory m_Tab = BuildCategory::Housing;
    uint8_t m_Rotation = 0;
    PlacementCheck m_LastCheck;
    GameObjectId m_HoveredBuilding = INVALID_GAME_OBJECT;
    GameObjectId m_InspectedBuilding = INVALID_GAME_OBJECT;
    BuildPreview m_Preview;
    bool m_HasPlacement = false;
    glm::ivec2 m_PreviewMinTile = glm::ivec2(0);
    glm::ivec2 m_PreviewTiles = glm::ivec2(0);
    bool m_PreviewConnected = false;
    bool m_PreviewInMarket = false;

    // Location preview, recomputed only when its inputs change
    struct LocationKey {
        int type = -1;
        uint8_t rotation = 0;
        glm::ivec2 minTile = glm::ivec2(0);
        uint32_t trees = 0, roads = 0, buildings = 0;
        bool operator==(const LocationKey&) const = default;
    };
    bool m_HasLocation = false;
    LocationKey m_LocationKey;
    LocationReport m_Location;
    std::vector<glm::ivec2> m_LocationTiles;
    uint32_t m_LocationRevision = 0;
    std::vector<uint8_t> m_LookBuffer; // Reused for every placement

    bool m_LeftWasPressed = false, m_RightWasPressed = false, m_RWasPressed = false;
    std::array<bool, 9> m_NumberWasPressed = {}; // Keys 1-9
    bool m_TabWasPressed = false;
};
