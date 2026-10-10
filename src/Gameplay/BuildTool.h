#pragma once

#include "Gameplay/Picking.h"
#include "Rendering/BuildPreview.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/GameObjects.h"
#include "Simulation/Placement.h"
#include "Simulation/ProducerLocation.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

struct GLFWwindow;
class BuildingModelLibrary;
class RoadTool;
class SmokeSystem;
class TerrainGenerator;
class Simulation;
class VoxelWorld;
class WorldEditor;

// Placing and demolishing buildings from the strategy camera, and the build selection (a building
// type, the road tool, or nothing). With a building type selected the footprint follows the
// cursor, snapped to the tile grid; R rotates it and left click places it when the preview is
// green. With nothing selected, right click demolishes the building or road tile under the cursor,
// and holding the left button on a building and dragging moves it: it lifts out of the world, its
// ghost follows the cursor (R rotates it), and letting go sets it down there when the ghost is green
// (free of charge, on the same island) or back where it stood otherwise; right click cancels.
// Placing a farm selects its module type next, so its pens go around it; demolishing a farm
// demolishes its modules. A moved farm carries its modules along (turning with it): set down, any
// whose new spot is blocked (a tree, a building) is destroyed, as in Anno. A newly placed building goes up over CONSTRUCTION_SECONDS: a timber frame rises from the ground
// with the finished layers behind it, raising dust. Only its look: it works from the start.
// A demolished building is gone at once, but its look comes down over DEMOLITION_SECONDS: it
// collapses into rubble, in a cloud of dust, and the rubble sinks away.
class BuildTool {
public:
    static constexpr int NO_TYPE = -1;
    static constexpr int ROAD = -2; // The road tool is selected (RoadTool handles the mouse)
    static constexpr float CONSTRUCTION_SECONDS = 3.0f;
    static constexpr int MAX_CONSTRUCTIONS = 64; // More at once are finished straight away
    static constexpr float DEMOLITION_SECONDS = 2.0f;
    static constexpr int MAX_DEMOLITIONS = 64;   // More at once vanish straight away

    // Build menu tabs: the buildable types of a category (hotkeys 1-9, 0), houses and service
    // buildings first; the Infrastructure tab ends with the road
    static int EntryCount(BuildCategory tab);
    static int EntryAt(BuildCategory tab, int index); // Building type, or ROAD

    BuildCategory Tab() const { return m_Tab; }
    void SetTab(BuildCategory tab) {
        if (TabUnlocked(tab)) m_Tab = tab;
    }
    // A tier's tab opens once any island has had residents of that tier (it stays open)
    bool TabUnlocked(BuildCategory tab) const { return tab == BuildCategory::Infrastructure || (int)tab <= m_UnlockedTier; }

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
    GameObjectId MovingBuilding() const { return m_Moving; } // Being dragged; INVALID when none
    bool IsUnderConstruction(GameObjectId id) const {
        return std::any_of(m_Constructions.begin(), m_Constructions.end(), [id](const Construction& c) { return c.id == id; });
    }

    // The building clicked with nothing selected (its panel is open); INVALID when none
    GameObjectId InspectedBuilding() const { return m_InspectedBuilding; }
    void ClearInspection() { m_InspectedBuilding = INVALID_GAME_OBJECT; }

    // Producers: how the previewed spot does on the chain's location rule, and the tiles that count
    bool HasLocationPreview() const { return m_HasLocation; }
    const LocationReport& PreviewLocation() const { return m_Location; }
    const std::vector<glm::ivec2>& PreviewLocationTiles() const { return m_LocationTiles; }
    uint32_t LocationRevision() const { return m_LocationRevision; } // Changes when the above or below do

    // While a farm is moved: the tiles its modules would take, and whether each module fits there
    const std::vector<glm::ivec2>& CarriedTiles() const { return m_CarriedTiles; }
    const std::vector<uint8_t>& CarriedValid() const { return m_CarriedValid; } // Per tile

    GameObjectId Place(uint16_t type, uint8_t rotation, glm::ivec2 minTile); // INVALID if not placeable
    void Demolish(GameObjectId id);

    // Selects the module type of a farm, to place its modules (they go to this farm first)
    void SelectModules(GameObjectId farm);

    // Rebuilds the voxels of a building whose type changed in place (house upgrades)
    void RefreshLook(GameObjectId id);

    // Once per frame (real time): buildings going up and coming down. dust: where their dust goes, null for none.
    void AnimateBuildings(float deltaTime, SmokeSystem* dust);

private:
    // Puts the generated terrain back in a box (under a demolished dock)
    void RestoreTerrain(glm::ivec3 minCorner, glm::ivec3 size);
    void ClearLook(GameObjectId id); // Its voxels out of the world, the ground under a dock back
    void ClearBox(const VoxelAnchorComponent& anchor, const BuildingType& building); // The same, by place
    void StampLook(GameObjectId id); // Its model into the world at its anchor
    void StartMove(GameObjectId id);
    void EndMove(bool toPreview);    // Set down at the last green preview, or back where it stood

    // A moved farm's modules, lifted with it, and where each stood
    struct Carried {
        GameObjectId id;
        glm::ivec2 minTile;
        uint8_t rotation;
    };
    // Where a carried module goes when its farm is set down at farmTile with farmRotation
    void CarriedTarget(const Carried& carried, glm::ivec2 farmTile, uint8_t farmRotation, glm::ivec2& minTile, uint8_t& rotation) const;
    bool CarriedFits(GameObjectId id, glm::ivec2 minTile, uint8_t rotation) const;
    void DestroyLifted(GameObjectId id); // A carried module that does not fit: gone, half its cost back

    const VoxelWorld& m_World;
    WorldEditor& m_Editor;
    Simulation& m_Simulation;
    RoadTool& m_RoadTool;
    const BuildingModelLibrary& m_Models;
    TerrainGenerator& m_Terrain;
    std::vector<uint8_t> m_ChunkScratch;   // One generated chunk (RestoreTerrain)
    std::vector<uint8_t> m_RestoreBuffer;  // The restored box

    struct Construction {
        GameObjectId id;
        float progress; // 0 to 1
    };
    std::vector<Construction> m_Constructions;
    struct Demolition {
        VoxelAnchorComponent anchor; // The object is gone: where it stood and what it looked like
        uint16_t type;
        uint8_t variant, rotation;
        float progress; // 0 to 1
    };
    std::vector<Demolition> m_Demolitions;
    std::vector<uint8_t> m_ConstructionBuffer;

    int m_SelectedType = NO_TYPE;
    GameObjectId m_ModuleFarm = INVALID_GAME_OBJECT; // The farm SelectModules chose
    GameObjectId m_MoveOwner = INVALID_GAME_OBJECT;  // A moved module's farm
    BuildCategory m_Tab = BuildCategory::Farmers;
    int m_UnlockedTier = 0; // The highest tier any island has had residents of
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
    std::vector<uint8_t> m_GhostBuffer; // The selected type's model, for the placement preview
    int m_GhostType = -1;
    uint8_t m_GhostRotation = 0, m_GhostVariant = 0;
    uint32_t m_GhostRevision = 0;

    // Moving a building
    GameObjectId m_PressedBuilding = INVALID_GAME_OBJECT; // Under the cursor when the left button went down
    glm::ivec2 m_PressedTile = glm::ivec2(0);
    GameObjectId m_Moving = INVALID_GAME_OBJECT;
    glm::ivec2 m_MoveFrom = glm::ivec2(0);  // Where it stood
    uint8_t m_MoveFromRotation = 0;
    bool m_MoveValid = false;               // The last preview was green, at m_MoveTile
    glm::ivec2 m_MoveTile = glm::ivec2(0);
    std::vector<Carried> m_Carried;
    std::vector<glm::ivec2> m_CarriedTiles;
    std::vector<uint8_t> m_CarriedValid;
    glm::ivec3 m_CarriedKey = glm::ivec3(-1); // Farm tile and rotation the above were made for

    bool m_LeftWasPressed = false, m_RightWasPressed = false, m_RWasPressed = false;
    std::array<bool, 10> m_NumberWasPressed = {}; // Keys 1-9, 0
    bool m_TabWasPressed = false;
};
