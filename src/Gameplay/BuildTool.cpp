#include "Gameplay/BuildTool.h"

#include "Simulation/BuildingLook.h"
#include "Simulation/Simulation.h"
#include "World/BlockTypes.h"
#include "World/WorldEditor.h"

#include <GLFW/glfw3.h>

#include <algorithm>

namespace {

// Edge-triggered press: true only on the frame the key or button goes down
bool Pressed(bool down, bool& wasDown) {
    bool edge = down && !wasDown;
    wasDown = down;
    return edge;
}

} // namespace

BuildTool::BuildTool(const VoxelWorld& world, WorldEditor& editor, Simulation& simulation)
    : m_World(world), m_Editor(editor), m_Simulation(simulation) {
    size_t largest = 0;
    for (const BuildingType& type : BUILDING_TYPES) {
        glm::ivec2 columns = FootprintColumns(type, 0);
        largest = std::max(largest, (size_t)columns.x * columns.y * BuildingHeight(type));
    }
    m_LookBuffer.reserve(largest);
}

void BuildTool::Update(GLFWwindow* window, const PickResult& hover, bool mouseFree, bool keyboardFree) {
    // Hotkeys (shown on the build menu buttons): 1 / 2 pick a building
    bool key1 = keyboardFree && glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool key2 = keyboardFree && glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    if (Pressed(key1, m_Key1WasPressed)) m_SelectedType = BUILDING_WAREHOUSE;
    if (Pressed(key2, m_Key2WasPressed)) m_SelectedType = BUILDING_FARMER_HOUSE;
    if (Pressed(keyboardFree && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS, m_RWasPressed)) m_Rotation = (m_Rotation + 1) & 3;
    bool leftClick = Pressed(mouseFree && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS, m_LeftWasPressed);
    bool rightClick = Pressed(mouseFree && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS, m_RightWasPressed);

    m_Preview = BuildPreview();
    m_LastCheck = PlacementCheck();
    m_HoveredBuilding = INVALID_GAME_OBJECT;
    if (!hover.hit) {
        if (rightClick) m_SelectedType = NO_TYPE;
        return;
    }

    glm::ivec2 hoverTile(ColumnToTile(hover.voxel.x), ColumnToTile(hover.voxel.z));
    GameObjectId under = m_Simulation.Occupancy().At(hoverTile);
    if (m_Simulation.Objects().IsAlive(under)) m_HoveredBuilding = under;

    if (rightClick) {
        if (m_HoveredBuilding != INVALID_GAME_OBJECT) {
            Demolish(m_HoveredBuilding);
            m_HoveredBuilding = INVALID_GAME_OBJECT;
        } else {
            m_SelectedType = NO_TYPE;
        }
    }

    if (m_SelectedType != NO_TYPE) {
        // Footprint centered on the hovered tile
        const BuildingType& type = BUILDING_TYPES[m_SelectedType];
        glm::ivec2 tiles = FootprintTiles(type, m_Rotation);
        glm::ivec2 minTile = hoverTile - tiles / 2;
        m_LastCheck = ValidatePlacement((uint16_t)m_SelectedType, m_Rotation, minTile, m_World, m_Simulation.Islands(),
            m_Simulation.Occupancy());

        if (leftClick && m_LastCheck.error == PlacementError::None) {
            m_HoveredBuilding = Place((uint16_t)m_SelectedType, m_Rotation, minTile);
            m_LastCheck.error = PlacementError::Occupied; // The spot is now taken
        }

        m_Preview.state = m_LastCheck.error == PlacementError::None ? BuildPreview::VALID : BuildPreview::INVALID;
        m_Preview.min = glm::ivec3(minTile.x * TILE_SIZE, BUILD_GROUND_Y, minTile.y * TILE_SIZE);
        m_Preview.max = m_Preview.min + glm::ivec3(tiles.x * TILE_SIZE, BuildingHeight(type), tiles.y * TILE_SIZE);
    } else if (m_HoveredBuilding != INVALID_GAME_OBJECT) {
        const VoxelAnchorComponent& anchor = m_Simulation.Objects().Anchor(m_HoveredBuilding);
        const BuildingType& type = BUILDING_TYPES[m_Simulation.Objects().Building(m_HoveredBuilding).type];
        m_Preview.state = BuildPreview::SELECTED;
        m_Preview.min = anchor.origin;
        m_Preview.max = anchor.origin + glm::ivec3(anchor.footprint.x, BuildingHeight(type), anchor.footprint.y);
    }
}

GameObjectId BuildTool::Place(uint16_t type, uint8_t rotation, glm::ivec2 minTile) {
    PlacementCheck check = ValidatePlacement(type, rotation, minTile, m_World, m_Simulation.Islands(), m_Simulation.Occupancy());
    if (check.error != PlacementError::None) return INVALID_GAME_OBJECT;

    GameObjectRegistry& objects = m_Simulation.Objects();
    GameObjectId id = objects.Create();
    if (id == INVALID_GAME_OBJECT) return INVALID_GAME_OBJECT;

    const BuildingType& building = BUILDING_TYPES[type];
    glm::ivec2 tiles = FootprintTiles(building, rotation);
    BuildingComponent& component = objects.Building(id);
    component.type = type;
    component.island = check.island;
    component.rotation = rotation;
    VoxelAnchorComponent& anchor = objects.Anchor(id);
    anchor.origin = glm::ivec3(minTile.x * TILE_SIZE, BUILD_GROUND_Y, minTile.y * TILE_SIZE);
    anchor.footprint = tiles * TILE_SIZE;
    m_Simulation.Occupancy().Occupy(minTile, tiles, id);

    BuildLook(building, rotation, m_LookBuffer);
    m_Editor.WriteBox(anchor.origin, glm::ivec3(anchor.footprint.x, BuildingHeight(building), anchor.footprint.y), m_LookBuffer);
    return id;
}

// Clears the building's voxels (the ground under it was never changed) and frees its tiles
void BuildTool::Demolish(GameObjectId id) {
    GameObjectRegistry& objects = m_Simulation.Objects();
    if (!objects.IsAlive(id)) return;

    const VoxelAnchorComponent anchor = objects.Anchor(id);
    const BuildingType& building = BUILDING_TYPES[objects.Building(id).type];
    m_Editor.FillBox(anchor.origin, glm::ivec3(anchor.footprint.x, BuildingHeight(building), anchor.footprint.y), Block::AIR);

    glm::ivec2 minTile(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
    m_Simulation.Occupancy().Release(minTile, anchor.footprint / TILE_SIZE, id);
    objects.Destroy(id);
}
