#include "Gameplay/BuildTool.h"

#include "Gameplay/RoadTool.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingModels.h"
#include "Simulation/Logistics.h"
#include "Simulation/Simulation.h"
#include "World/BlockTypes.h"
#include "World/Chunk.h"
#include "World/TerrainGenerator.h"
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

BuildTool::BuildTool(const VoxelWorld& world, WorldEditor& editor, Simulation& simulation, RoadTool& roads, const BuildingModelLibrary& models,
    TerrainGenerator& terrain)
    : m_World(world), m_Editor(editor), m_Simulation(simulation), m_RoadTool(roads), m_Models(models), m_Terrain(terrain) {
    size_t largest = 0;
    for (const BuildingType& type : BUILDING_TYPES) {
        glm::ivec2 columns = FootprintColumns(type, 0);
        largest = std::max(largest, (size_t)columns.x * columns.y * BuildingVolumeHeight(type));
    }
    m_LookBuffer.reserve(largest);
    m_RestoreBuffer.reserve(largest);
    m_ChunkScratch.reserve(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
    m_LocationTiles.reserve(1024);
}

int BuildTool::EntryCount(BuildCategory tab) {
    int count = tab == BuildCategory::Infrastructure ? 1 : 0; // The road
    for (const BuildingType& type : BUILDING_TYPES) count += (type.buildable && type.category == tab) ? 1 : 0;
    return count;
}

int BuildTool::EntryAt(BuildCategory tab, int index) {
    for (int type = 0; type < (int)BUILDING_TYPES.size(); type++) {
        if (!BUILDING_TYPES[type].buildable || BUILDING_TYPES[type].category != tab) continue;
        if (index-- == 0) return type;
    }
    if (tab == BuildCategory::Infrastructure && index == 0) return ROAD;
    return NO_TYPE;
}

// The tallest building, for clearing the space of one whose look changes
static int MaxBuildingHeight() {
    int height = 0;
    for (const BuildingType& type : BUILDING_TYPES) height = std::max(height, BuildingHeight(type));
    return height;
}

void BuildTool::SelectType(int type) {
    if (m_SelectedType == ROAD && type != ROAD) m_RoadTool.Cancel();
    m_SelectedType = type;
}

void BuildTool::Update(GLFWwindow* window, const PickResult& hover, bool mouseFree, bool keyboardFree) {
    // Hotkeys (shown on the build menu buttons): Tab switches tabs, 1, 2, ... pick from the open tab
    if (Pressed(keyboardFree && glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS, m_TabWasPressed)) {
        m_Tab = (BuildCategory)(((int)m_Tab + 1) % (int)BuildCategory::Count);
    }
    int entries = EntryCount(m_Tab);
    for (int key = 0; key < (int)m_NumberWasPressed.size(); key++) {
        bool down = keyboardFree && glfwGetKey(window, GLFW_KEY_1 + key) == GLFW_PRESS;
        if (Pressed(down, m_NumberWasPressed[key]) && key < entries) SelectType(EntryAt(m_Tab, key));
    }
    if (Pressed(keyboardFree && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS, m_RWasPressed)) m_Rotation = (m_Rotation + 1) & 3;
    bool leftClick = Pressed(mouseFree && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS, m_LeftWasPressed);
    bool rightClick = Pressed(mouseFree && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS, m_RightWasPressed);

    m_Preview = BuildPreview();
    m_LastCheck = PlacementCheck();
    m_HasPlacement = false;
    m_PreviewConnected = false;
    m_PreviewInMarket = false;
    bool hadLocation = m_HasLocation;
    m_HasLocation = false;
    m_HoveredBuilding = INVALID_GAME_OBJECT;

    // The road tool owns the mouse while it is selected
    if (m_SelectedType == ROAD) {
        m_RoadTool.Update(window, hover, mouseFree);
        if (m_RoadTool.ConsumeCancelRequest()) SelectType(NO_TYPE);
        return;
    }

    if (!hover.hit) {
        if (rightClick) SelectType(NO_TYPE);
        if (leftClick && m_SelectedType == NO_TYPE) m_InspectedBuilding = INVALID_GAME_OBJECT;
        if (hadLocation) m_LocationRevision++; // The preview went away
        return;
    }

    glm::ivec2 hoverTile(ColumnToTile(hover.voxel.x), ColumnToTile(hover.voxel.z));
    GameObjectId under = m_Simulation.Occupancy().At(hoverTile);
    if (m_Simulation.Objects().IsAlive(under)) m_HoveredBuilding = under;

    // Nothing selected: a click opens the building's panel, or closes it on open ground
    if (leftClick && m_SelectedType == NO_TYPE) m_InspectedBuilding = m_HoveredBuilding;

    if (rightClick) {
        if (m_HoveredBuilding != INVALID_GAME_OBJECT) {
            Demolish(m_HoveredBuilding);
            m_HoveredBuilding = INVALID_GAME_OBJECT;
        } else if (m_SelectedType != NO_TYPE) {
            SelectType(NO_TYPE);
        } else {
            m_RoadTool.Demolish(hoverTile); // Nothing happens if it is not road
        }
    }

    if (m_SelectedType >= 0) {
        // Footprint centered on the hovered tile
        const BuildingType& type = BUILDING_TYPES[m_SelectedType];
        glm::ivec2 tiles = FootprintTiles(type, m_Rotation);
        glm::ivec2 minTile = hoverTile - tiles / 2;
        m_LastCheck = ValidatePlacement(m_Simulation.MakePlacementContext(m_World), (uint16_t)m_SelectedType, m_Rotation, minTile);
        if (m_LastCheck.error == PlacementError::None) {
            m_LastCheck.error = m_Simulation.Coins().Check((uint16_t)m_SelectedType, m_LastCheck.island, m_Simulation.Economy());
        }
        m_PreviewConnected = LogisticsSystem::ConnectionOf(m_Simulation.Roads(), minTile, tiles).connected;
        m_PreviewInMarket = LogisticsSystem::MarketConnectionOf(m_Simulation.Roads(), minTile, tiles).connected;

        // Producers: what the location rule makes of this spot
        if (type.role == BuildingRole::Producer) {
            LocationKey key{ m_SelectedType, m_Rotation, minTile, m_Simulation.Trees().Revision(), m_Simulation.Roads().Revision(),
                m_Simulation.BuildingsRevision() };
            if (!hadLocation || !(key == m_LocationKey)) {
                m_LocationKey = key;
                m_Location = EvaluateLocation(PRODUCTION_CHAINS[type.chain], minTile, tiles, m_Simulation.Islands(), m_Simulation.Occupancy(),
                    m_Simulation.Roads(), m_Simulation.Trees(), &m_LocationTiles);
                m_LocationRevision++;
            }
            m_HasLocation = true;
        }

        if (leftClick && m_LastCheck.error == PlacementError::None) {
            m_HoveredBuilding = Place((uint16_t)m_SelectedType, m_Rotation, minTile);
            m_LastCheck.error = PlacementError::Occupied; // The spot is now taken
        }

        m_HasPlacement = true;
        m_PreviewMinTile = minTile;
        m_PreviewTiles = tiles;
        m_Preview.state = m_LastCheck.error == PlacementError::None ? BuildPreview::VALID : BuildPreview::INVALID;
        m_Preview.min = glm::ivec3(minTile.x * TILE_SIZE, BUILD_GROUND_Y - type.belowGround, minTile.y * TILE_SIZE);
        m_Preview.max = m_Preview.min + glm::ivec3(tiles.x * TILE_SIZE, BuildingVolumeHeight(type), tiles.y * TILE_SIZE);
    } else if (m_HoveredBuilding != INVALID_GAME_OBJECT) {
        const VoxelAnchorComponent& anchor = m_Simulation.Objects().Anchor(m_HoveredBuilding);
        const BuildingType& type = BUILDING_TYPES[m_Simulation.Objects().Building(m_HoveredBuilding).type];
        m_Preview.state = BuildPreview::SELECTED;
        m_Preview.min = anchor.origin - glm::ivec3(0, type.belowGround, 0);
        m_Preview.max = m_Preview.min + glm::ivec3(anchor.footprint.x, BuildingVolumeHeight(type), anchor.footprint.y);
    }
}

GameObjectId BuildTool::Place(uint16_t type, uint8_t rotation, glm::ivec2 minTile) {
    PlacementCheck check = ValidatePlacement(m_Simulation.MakePlacementContext(m_World), type, rotation, minTile);
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
    component.variant = m_Models.PickVariant(type, id);
    VoxelAnchorComponent& anchor = objects.Anchor(id);
    anchor.origin = glm::ivec3(minTile.x * TILE_SIZE, BUILD_GROUND_Y, minTile.y * TILE_SIZE);
    anchor.footprint = tiles * TILE_SIZE;
    m_Simulation.Occupancy().Occupy(minTile, tiles, id);
    if (type == BUILDING_WAREHOUSE) m_Simulation.Economy().OnWarehouseAdded(check.island);
    m_Simulation.Coins().Pay(type, check.island, m_Simulation.Economy());
    m_Simulation.MarkBuildingsChanged();

    m_Models.BuildVoxels(type, component.variant, rotation, m_LookBuffer);
    glm::ivec3 volumeOrigin = anchor.origin - glm::ivec3(0, building.belowGround, 0);
    m_Editor.WriteBox(volumeOrigin, glm::ivec3(anchor.footprint.x, BuildingVolumeHeight(building), anchor.footprint.y), m_LookBuffer,
        building.belowGround);
    return id;
}

void BuildTool::RefreshLook(GameObjectId id) {
    GameObjectRegistry& objects = m_Simulation.Objects();
    if (!objects.IsAlive(id)) return;
    const VoxelAnchorComponent anchor = objects.Anchor(id);
    const BuildingComponent component = objects.Building(id);
    const BuildingType& building = BUILDING_TYPES[component.type];

    // Clear the tallest a building can be (the old look may have been taller), then stamp the new one
    m_Editor.FillBox(anchor.origin, glm::ivec3(anchor.footprint.x, MaxBuildingHeight(), anchor.footprint.y), Block::AIR);
    m_Models.BuildVoxels(component.type, component.variant, component.rotation, m_LookBuffer);
    glm::ivec3 volumeOrigin = anchor.origin - glm::ivec3(0, building.belowGround, 0);
    m_Editor.WriteBox(volumeOrigin, glm::ivec3(anchor.footprint.x, BuildingVolumeHeight(building), anchor.footprint.y), m_LookBuffer,
        building.belowGround);
}

void BuildTool::RestoreTerrain(glm::ivec3 minCorner, glm::ivec3 size) {
    m_RestoreBuffer.assign((size_t)size.x * size.y * size.z, Block::AIR);
    glm::ivec3 maxCorner = minCorner + size - 1;
    for (int cy = minCorner.y >> 5; cy <= maxCorner.y >> 5; cy++) {
        for (int cz = minCorner.z >> 5; cz <= maxCorner.z >> 5; cz++) {
            for (int cx = minCorner.x >> 5; cx <= maxCorner.x >> 5; cx++) {
                m_Terrain.GenerateChunk(cx, cy, cz, m_ChunkScratch);
                glm::ivec3 from = glm::max(minCorner, glm::ivec3(cx, cy, cz) * CHUNK_SIZE);
                glm::ivec3 to = glm::min(maxCorner, glm::ivec3(cx, cy, cz) * CHUNK_SIZE + (CHUNK_SIZE - 1));
                for (int y = from.y; y <= to.y; y++) {
                    for (int z = from.z; z <= to.z; z++) {
                        for (int x = from.x; x <= to.x; x++) {
                            glm::ivec3 local = glm::ivec3(x, y, z) - minCorner;
                            m_RestoreBuffer[(size_t)local.x + (size_t)size.x * ((size_t)local.z + (size_t)size.z * (size_t)local.y)] =
                                m_ChunkScratch[LocalIndex(x & 31, y & 31, z & 31)];
                        }
                    }
                }
            }
        }
    }
    m_Editor.WriteBox(minCorner, size, m_RestoreBuffer);
}

// Clears the building's voxels (the ground under it was never changed) and frees its tiles
void BuildTool::Demolish(GameObjectId id) {
    GameObjectRegistry& objects = m_Simulation.Objects();
    if (!objects.IsAlive(id)) return;

    const VoxelAnchorComponent anchor = objects.Anchor(id);
    const BuildingComponent component = objects.Building(id);
    const BuildingType& building = BUILDING_TYPES[component.type];
    m_Editor.FillBox(anchor.origin, glm::ivec3(anchor.footprint.x, BuildingHeight(building), anchor.footprint.y), Block::AIR);
    if (building.belowGround > 0) {
        // Pilings and hulls stood in the ground or the sea: put the generated terrain back
        RestoreTerrain(anchor.origin - glm::ivec3(0, building.belowGround, 0), glm::ivec3(anchor.footprint.x, building.belowGround, anchor.footprint.y));
    }

    glm::ivec2 minTile(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
    m_Simulation.Occupancy().Release(minTile, anchor.footprint / TILE_SIZE, id);
    m_Simulation.Coins().Refund(component.type, component.island, m_Simulation.Economy());
    if (component.type == BUILDING_WAREHOUSE) m_Simulation.Economy().OnWarehouseRemoved(component.island);
    objects.Destroy(id);
    m_Simulation.MarkBuildingsChanged();
}
