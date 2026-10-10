#include "Gameplay/BuildTool.h"

#include "Gameplay/RoadTool.h"
#include "Gameplay/Smoke.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingModels.h"
#include "Simulation/Logistics.h"
#include "Simulation/ProducerLocation.h"
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
    m_GhostBuffer.reserve(largest);
    m_RestoreBuffer.reserve(largest);
    m_ConstructionBuffer.reserve(largest);
    m_Constructions.reserve(MAX_CONSTRUCTIONS);
    m_Demolitions.reserve(MAX_DEMOLITIONS);
    m_Carried.reserve(8);
    m_CarriedTiles.reserve(64);
    m_CarriedValid.reserve(64);
    m_ChunkScratch.reserve(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
    m_LocationTiles.reserve(1024);
}

int BuildTool::EntryCount(BuildCategory tab) {
    int count = tab == BuildCategory::Infrastructure ? 1 : 0; // The road
    for (const BuildingType& type : BUILDING_TYPES) count += (type.buildable && type.category == tab) ? 1 : 0;
    return count;
}

int BuildTool::EntryAt(BuildCategory tab, int index) {
    for (int pass = 0; pass < 2; pass++) { // Houses and service buildings, then the rest
        for (int type = 0; type < (int)BUILDING_TYPES.size(); type++) {
            const BuildingType& building = BUILDING_TYPES[type];
            if (!building.buildable || building.category != tab) continue;
            bool first = building.role == BuildingRole::Residence || building.role == BuildingRole::Service;
            if (first == (pass == 0) && index-- == 0) return type;
        }
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
    if (!BuildingUnlocked(type) && m_SelectedType != type) return; // Shown in the menu, not buildable yet
    if (m_SelectedType == ROAD && type != ROAD) m_RoadTool.Cancel();
    if (m_Moving != INVALID_GAME_OBJECT) EndMove(false);
    m_SelectedType = type;
    m_ModuleFarm = INVALID_GAME_OBJECT;
}

bool BuildTool::Rotate(int quarterTurns) {
    if (m_SelectedType < 0 && m_Moving == INVALID_GAME_OBJECT) return false;
    m_Rotation = (uint8_t)((m_Rotation + quarterTurns) & 3);
    return true;
}

bool BuildTool::Cancel() {
    if (m_Moving == INVALID_GAME_OBJECT && m_SelectedType == NO_TYPE) return false;
    SelectType(NO_TYPE);
    return true;
}

void BuildTool::SelectModules(GameObjectId farm) {
    if (!m_Simulation.Objects().IsAlive(farm)) return;
    int module = ModuleTypeOf(m_Simulation.Objects().Building(farm).type);
    if (module < 0) return;
    SelectType(module);
    m_ModuleFarm = farm;
}

void BuildTool::Update(GLFWwindow* window, const PickResult& hover, bool mouseFree, bool keyboardFree) {
    // Tabs of the tiers reached so far
    for (size_t i = 0; i < m_Simulation.Economy().IslandSlotCount(); i++) {
        const IslandStorage& storage = m_Simulation.Economy().IslandAt(i);
        for (int tier = m_UnlockedTier + 1; tier < TIER_COUNT; tier++) {
            if (storage.population[tier] > 0) m_UnlockedTier = tier;
        }
    }

    // Hotkeys (shown on the build menu buttons): B opens and closes the construction menu; while it
    // is open Tab switches to the next tab and 1-9 and 0 pick from the open tab
    if (Pressed(keyboardFree && glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS, m_BWasPressed)) ToggleMenu();
    if (Pressed(keyboardFree && m_MenuOpen && glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS, m_TabWasPressed)) {
        m_Tab = (BuildCategory)(((int)m_Tab + 1) % (int)BuildCategory::Count);
    }
    int entries = EntryCount(m_Tab);
    for (int key = 0; key < (int)m_NumberWasPressed.size(); key++) {
        bool down = keyboardFree && m_MenuOpen && glfwGetKey(window, key == 9 ? GLFW_KEY_0 : GLFW_KEY_1 + key) == GLFW_PRESS;
        if (Pressed(down, m_NumberWasPressed[key]) && key < entries) SelectType(EntryAt(m_Tab, key));
    }
    if (Pressed(keyboardFree && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS, m_RWasPressed)) m_Rotation = (m_Rotation + 1) & 3;
    // Tools: a second press puts them away
    auto toggle = [this](int tool) { SelectType(m_SelectedType == tool ? NO_TYPE : tool); };
    if (Pressed(keyboardFree && glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS, m_DeleteWasPressed)) toggle(DEMOLISH);
    if (Pressed(keyboardFree && glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS, m_MWasPressed)) toggle(MOVE);
    if (Pressed(keyboardFree && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS, m_CWasPressed)) toggle(COPY);
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

    // A building being moved is set down when the left button is let go (or clicked, when the move
    // tool picked it up); right click cancels
    GameObjectRegistry& objects = m_Simulation.Objects();
    bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (!leftDown) m_PressedBuilding = INVALID_GAME_OBJECT;
    bool setDown = m_ClickMove ? leftClick : !leftDown;
    if (m_Moving != INVALID_GAME_OBJECT && (rightClick || setDown || !objects.IsAlive(m_Moving))) {
        EndMove(!rightClick && m_MoveValid);
        return;
    }

    if (!hover.hit) {
        m_MoveValid = false;
        if (m_Moving != INVALID_GAME_OBJECT) return;
        if (rightClick) SelectType(NO_TYPE);
        if (leftClick && m_SelectedType == NO_TYPE) m_InspectedBuilding = INVALID_GAME_OBJECT;
        if (rightClick && m_SelectedType == NO_TYPE) m_InspectedBuilding = INVALID_GAME_OBJECT;
        if (hadLocation) m_LocationRevision++; // The preview went away
        return;
    }

    glm::ivec2 hoverTile(ColumnToTile(hover.voxel.x), ColumnToTile(hover.voxel.z));
    GameObjectId under = m_Simulation.Occupancy().At(hoverTile);
    if (m_Simulation.Objects().IsAlive(under)) m_HoveredBuilding = under;

    // Demolish tool: the building under the cursor outlined in red; buildings and road tiles clicked
    // or dragged over come down
    if (m_SelectedType == DEMOLISH) {
        if (rightClick) {
            SelectType(NO_TYPE);
            return;
        }
        if (mouseFree && leftDown) {
            if (m_HoveredBuilding != INVALID_GAME_OBJECT) {
                Demolish(m_HoveredBuilding);
                m_HoveredBuilding = INVALID_GAME_OBJECT;
            } else {
                m_RoadTool.Demolish(hoverTile); // Nothing happens if it is not road
            }
        }
        if (m_HoveredBuilding != INVALID_GAME_OBJECT) {
            const VoxelAnchorComponent& anchor = objects.Anchor(m_HoveredBuilding);
            const BuildingType& type = BUILDING_TYPES[objects.Building(m_HoveredBuilding).type];
            m_Preview.state = BuildPreview::INVALID; // No ghost: a red box
            m_Preview.min = anchor.origin - glm::ivec3(0, type.belowGround, 0);
            m_Preview.max = m_Preview.min + glm::ivec3(anchor.footprint.x, BuildingVolumeHeight(type), anchor.footprint.y);
        }
        return;
    }
    // Copy tool: the clicked building's type and rotation are selected (an upgraded house builds a
    // farmer house; a farm module its farm's modules)
    if (m_SelectedType == COPY && leftClick && m_HoveredBuilding != INVALID_GAME_OBJECT) {
        const BuildingComponent& building = objects.Building(m_HoveredBuilding);
        const BuildingType& type = BUILDING_TYPES[building.type];
        uint8_t rotation = building.rotation;
        if (type.role == BuildingRole::Module) {
            if (objects.IsAlive(building.owner)) SelectModules(building.owner);
        } else {
            SelectType(type.role == BuildingRole::Residence ? (int)RESIDENCE_FOR_TIER[0] : type.buildable ? (int)building.type : NO_TYPE);
        }
        m_Rotation = rotation;
        leftClick = false; // Not also a placement
    }
    // Move tool: a click picks the building up
    if (m_SelectedType == MOVE && leftClick && m_Moving == INVALID_GAME_OBJECT && m_HoveredBuilding != INVALID_GAME_OBJECT) {
        StartMove(m_HoveredBuilding);
        m_ClickMove = true;
        leftClick = false;
    }

    // Nothing selected: a click opens the building's panel, or closes it on open ground; holding the
    // button and dragging to another tile picks the building up
    if (leftClick && m_SelectedType == NO_TYPE) {
        m_InspectedBuilding = m_HoveredBuilding;
        m_PressedBuilding = m_HoveredBuilding;
        m_PressedTile = hoverTile;
    }
    if (m_Moving == INVALID_GAME_OBJECT && objects.IsAlive(m_PressedBuilding) && hoverTile != m_PressedTile) StartMove(m_PressedBuilding);
    bool moving = m_Moving != INVALID_GAME_OBJECT;

    // Right click drops the selection, or closes the building panel (demolishing is the demolish tool's)
    if (rightClick) {
        if (m_SelectedType != NO_TYPE) SelectType(NO_TYPE);
        else m_InspectedBuilding = INVALID_GAME_OBJECT;
    }

    // The type to preview: the selected one, or the building being moved
    int previewType = moving ? objects.Building(m_Moving).type : m_SelectedType;
    uint8_t variant = moving ? objects.Building(m_Moving).variant : 0;
    if (previewType >= 0) {
        // Footprint centered on the hovered tile
        const BuildingType& type = BUILDING_TYPES[previewType];
        glm::ivec2 tiles = FootprintTiles(type, m_Rotation);
        glm::ivec2 minTile = hoverTile - tiles / 2;
        m_LastCheck = ValidatePlacement(m_Simulation.MakePlacementContext(m_World), (uint16_t)previewType, m_Rotation, minTile);
        if (m_LastCheck.error == PlacementError::None && moving) {
            if (m_LastCheck.island != objects.Building(m_Moving).island) m_LastCheck.error = PlacementError::OtherIsland;
        } else if (m_LastCheck.error == PlacementError::None) {
            m_LastCheck.error = m_Simulation.Ships().CheckBuildCost((uint16_t)previewType, m_LastCheck.island, minTile, tiles,
                m_Simulation.Economy(), m_Simulation.Coins());
        }
        m_MoveValid = m_LastCheck.error == PlacementError::None;
        m_MoveTile = minTile;
        if (moving && !m_Carried.empty() && m_CarriedKey != glm::ivec3(minTile, m_Rotation)) {
            // Where the farm's modules would go, green or red
            m_CarriedKey = glm::ivec3(minTile, m_Rotation);
            m_CarriedTiles.clear();
            m_CarriedValid.clear();
            for (const Carried& carried : m_Carried) {
                glm::ivec2 moduleTile;
                uint8_t moduleRotation;
                CarriedTarget(carried, minTile, m_Rotation, moduleTile, moduleRotation);
                bool fits = CarriedFits(carried.id, moduleTile, moduleRotation);
                glm::ivec2 moduleTiles = FootprintTiles(BUILDING_TYPES[objects.Building(carried.id).type], moduleRotation);
                for (int z = 0; z < moduleTiles.y; z++) {
                    for (int x = 0; x < moduleTiles.x; x++) {
                        m_CarriedTiles.push_back(moduleTile + glm::ivec2(x, z)); // Within the reserve for 5 pens
                        m_CarriedValid.push_back(fits ? 1 : 0);
                    }
                }
            }
            m_LocationRevision++;
        }
        m_PreviewConnected = LogisticsSystem::ConnectionOf(m_Simulation.Roads(), minTile, tiles).connected;
        m_PreviewInMarket = LogisticsSystem::ServiceConnectionOf(m_Simulation.Roads(), ServiceType::Marketplace, minTile, tiles).connected;

        // Producers: what the location rule makes of this spot. Modules: their farm, its range and its modules.
        GameObjectId farm = INVALID_GAME_OBJECT;
        if (type.role == BuildingRole::Module) {
            farm = objects.IsAlive(m_ModuleFarm) ? m_ModuleFarm
                                                 : FindModuleFarm(objects, (uint16_t)previewType, m_LastCheck.island, minTile, tiles);
        }
        if (farm != INVALID_GAME_OBJECT) {
            const VoxelAnchorComponent& anchor = objects.Anchor(farm);
            glm::ivec2 farmMin(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
            LocationKey key{ previewType, m_Rotation, farmMin, m_Simulation.Trees().Revision(), m_Simulation.Roads().Revision(),
                m_Simulation.BuildingsRevision() };
            if (!hadLocation || !(key == m_LocationKey)) {
                m_LocationKey = key;
                const ProductionChain& chain = PRODUCTION_CHAINS[BUILDING_TYPES[objects.Building(farm).type].chain];
                m_Location = EvaluateLocation(chain, farmMin, anchor.footprint / TILE_SIZE, m_Simulation.Islands(), m_Simulation.Occupancy(),
                    m_Simulation.Roads(), m_Simulation.Trees(), &m_LocationTiles);
                m_Location.count = CountModules(objects, farm);
                m_LocationRevision++;
            }
            m_HasLocation = true;
        } else if (type.role == BuildingRole::Producer) {
            LocationKey key{ previewType, m_Rotation, minTile, m_Simulation.Trees().Revision(), m_Simulation.Roads().Revision(),
                m_Simulation.BuildingsRevision() };
            if (!hadLocation || !(key == m_LocationKey)) {
                m_LocationKey = key;
                m_Location = EvaluateLocation(PRODUCTION_CHAINS[type.chain], minTile, tiles, m_Simulation.Islands(), m_Simulation.Occupancy(),
                    m_Simulation.Roads(), m_Simulation.Trees(), &m_LocationTiles);
                m_LocationRevision++;
            }
            m_HasLocation = true;
        }

        if (!moving && leftClick && m_LastCheck.error == PlacementError::None) {
            m_HoveredBuilding = Place((uint16_t)previewType, m_Rotation, minTile);
            m_LastCheck.error = PlacementError::Occupied; // The spot is now taken
            if (ModuleTypeOf(previewType) >= 0) SelectModules(m_HoveredBuilding); // A farm: its pens next, as in Anno
        }

        m_HasPlacement = true;
        m_PreviewMinTile = minTile;
        m_PreviewTiles = tiles;
        m_Preview.state = m_LastCheck.error == PlacementError::None ? BuildPreview::VALID : BuildPreview::INVALID;
        m_Preview.min = glm::ivec3(minTile.x * TILE_SIZE, BUILD_GROUND_Y - type.belowGround, minTile.y * TILE_SIZE);
        m_Preview.max = m_Preview.min + glm::ivec3(tiles.x * TILE_SIZE, BuildingVolumeHeight(type), tiles.y * TILE_SIZE);
        if (m_GhostType != previewType || m_GhostRotation != m_Rotation || m_GhostVariant != variant) {
            m_GhostType = previewType;
            m_GhostRotation = m_Rotation;
            m_GhostVariant = variant;
            m_Models.BuildVoxels((uint16_t)previewType, variant, m_Rotation, m_GhostBuffer);
            m_GhostRevision++;
        }
        m_Preview.ghost = &m_GhostBuffer;
        m_Preview.ghostRevision = m_GhostRevision;
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
    if (building.role == BuildingRole::Module) component.owner = FindModuleFarm(objects, type, check.island, minTile, tiles, m_ModuleFarm);
    VoxelAnchorComponent& anchor = objects.Anchor(id);
    anchor.origin = glm::ivec3(minTile.x * TILE_SIZE, BUILD_GROUND_Y, minTile.y * TILE_SIZE);
    anchor.footprint = tiles * TILE_SIZE;
    m_Simulation.Occupancy().Occupy(minTile, tiles, id);
    m_Simulation.Ships().PayBuildCost(type, check.island, minTile, tiles, m_Simulation.Economy(), m_Simulation.Coins());
    if (building.role == BuildingRole::Storage) m_Simulation.Economy().OnWarehouseAdded(check.island);
    m_Simulation.MarkBuildingsChanged();
    if (m_Constructions.size() < (size_t)MAX_CONSTRUCTIONS) {
        m_Constructions.push_back({ id, 0.0f }); // Within the reserve
        AnimateBuildings(0.0f, nullptr);
    } else {
        StampLook(id);
    }
    return id;
}

void BuildTool::AnimateBuildings(float deltaTime, SmokeSystem* dust) {
    for (size_t i = 0; i < m_Demolitions.size();) {
        Demolition& demolition = m_Demolitions[i];
        const BuildingType& building = BUILDING_TYPES[demolition.type];
        demolition.progress += deltaTime / DEMOLITION_SECONDS;
        if (demolition.progress >= 1.0f) {
            ClearBox(demolition.anchor, building);
            demolition = m_Demolitions.back();
            m_Demolitions.pop_back();
            continue;
        }
        glm::ivec3 origin = demolition.anchor.origin - glm::ivec3(0, building.belowGround, 0);
        glm::ivec3 size(demolition.anchor.footprint.x, BuildingVolumeHeight(building), demolition.anchor.footprint.y);
        m_Models.BuildVoxels(demolition.type, demolition.variant, demolition.rotation, m_LookBuffer);
        DemolitionLook(m_LookBuffer, size, building.belowGround, demolition.progress, m_ConstructionBuffer);
        m_Editor.WriteBox(origin, size, m_ConstructionBuffer, building.belowGround);
        if (dust && demolition.progress < 0.7f) {
            // A thick cloud while it falls
            glm::vec2 min(demolition.anchor.origin.x, demolition.anchor.origin.z);
            dust->Dust(deltaTime * 4.0f, min, min + glm::vec2(demolition.anchor.footprint), (float)BUILD_GROUND_Y + 2.0f);
        }
        i++;
    }

    GameObjectRegistry& objects = m_Simulation.Objects();
    for (size_t i = 0; i < m_Constructions.size();) {
        Construction& construction = m_Constructions[i];
        construction.progress += deltaTime / CONSTRUCTION_SECONDS;
        if (construction.progress >= 1.0f) {
            StampLook(construction.id);
            construction = m_Constructions.back();
            m_Constructions.pop_back();
            continue;
        }
        const VoxelAnchorComponent& anchor = objects.Anchor(construction.id);
        const BuildingComponent& component = objects.Building(construction.id);
        const BuildingType& building = BUILDING_TYPES[component.type];
        glm::ivec3 origin = anchor.origin - glm::ivec3(0, building.belowGround, 0);
        glm::ivec3 size(anchor.footprint.x, BuildingVolumeHeight(building), anchor.footprint.y);
        m_Models.BuildVoxels(component.type, component.variant, component.rotation, m_LookBuffer);
        ConstructionLook(m_LookBuffer, size, construction.progress, m_ConstructionBuffer);
        m_Editor.WriteBox(origin, size, m_ConstructionBuffer, building.belowGround);
        if (dust) {
            float top = (float)origin.y + construction.progress * (float)size.y;
            dust->Dust(deltaTime, glm::vec2(origin.x, origin.z), glm::vec2(origin.x + size.x, origin.z + size.z), std::max(top, (float)BUILD_GROUND_Y));
        }
        i++;
    }
}

void BuildTool::StampLook(GameObjectId id) {
    const VoxelAnchorComponent& anchor = m_Simulation.Objects().Anchor(id);
    const BuildingComponent& component = m_Simulation.Objects().Building(id);
    const BuildingType& building = BUILDING_TYPES[component.type];
    m_Models.BuildVoxels(component.type, component.variant, component.rotation, m_LookBuffer);
    glm::ivec3 volumeOrigin = anchor.origin - glm::ivec3(0, building.belowGround, 0);
    m_Editor.WriteBox(volumeOrigin, glm::ivec3(anchor.footprint.x, BuildingVolumeHeight(building), anchor.footprint.y), m_LookBuffer,
        building.belowGround);
}

void BuildTool::RefreshLook(GameObjectId id) {
    GameObjectRegistry& objects = m_Simulation.Objects();
    if (!objects.IsAlive(id) || id == m_Moving) return; // A lifted building gets its new look when set down
    const VoxelAnchorComponent anchor = objects.Anchor(id);

    // Clear the tallest a building can be (the old look may have been taller), then stamp the new one
    m_Editor.FillBox(anchor.origin, glm::ivec3(anchor.footprint.x, MaxBuildingHeight(), anchor.footprint.y), Block::AIR);
    StampLook(id);
}

void BuildTool::StartMove(GameObjectId id) {
    const VoxelAnchorComponent& anchor = m_Simulation.Objects().Anchor(id);
    m_Moving = id;
    m_PressedBuilding = INVALID_GAME_OBJECT;
    m_MoveFrom = glm::ivec2(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
    m_MoveFromRotation = m_Simulation.Objects().Building(id).rotation;
    m_Rotation = m_MoveFromRotation;
    m_MoveValid = false;
    // A lifted module leaves its farm (so it has room for it again) and finds one when set down
    GameObjectRegistry& objects = m_Simulation.Objects();
    BuildingComponent& building = objects.Building(id);
    m_MoveOwner = building.owner;
    building.owner = INVALID_GAME_OBJECT;
    ClearLook(id);
    m_Simulation.LiftBuilding(id);

    // A farm lifts its modules with it
    m_Carried.clear();
    m_CarriedKey = glm::ivec3(-1);
    if (ModuleTypeOf(building.type) < 0) return;
    for (uint32_t slot = 0; slot < objects.SlotCount() && m_Carried.size() < m_Carried.capacity(); slot++) {
        GameObjectId module = objects.IdAtSlot(slot);
        if (module == INVALID_GAME_OBJECT || objects.Building(module).owner != id) continue;
        const VoxelAnchorComponent& anchor = objects.Anchor(module);
        m_Carried.push_back({ module, glm::ivec2(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z)), objects.Building(module).rotation });
        ClearLook(module);
        m_Simulation.LiftBuilding(module);
    }
}

void BuildTool::CarriedTarget(const Carried& carried, glm::ivec2 farmTile, uint8_t farmRotation, glm::ivec2& minTile, uint8_t& rotation) const {
    // Turn the module's rectangle about the farm's center, in doubled tile units so centers are whole
    const GameObjectRegistry& objects = m_Simulation.Objects();
    const BuildingType& farm = BUILDING_TYPES[objects.Building(m_Moving).type];
    const BuildingType& module = BUILDING_TYPES[objects.Building(carried.id).type];
    int turns = (farmRotation - m_MoveFromRotation) & 3;
    glm::ivec2 offset = carried.minTile * 2 + FootprintTiles(module, carried.rotation) - (m_MoveFrom * 2 + FootprintTiles(farm, m_MoveFromRotation));
    for (int i = 0; i < turns; i++) offset = glm::ivec2(-offset.y, offset.x); // A quarter turn: -z becomes +x
    rotation = (uint8_t)((carried.rotation + turns) & 3);
    minTile = (farmTile * 2 + FootprintTiles(farm, farmRotation) + offset - FootprintTiles(module, rotation)) / 2;
}

bool BuildTool::CarriedFits(GameObjectId id, glm::ivec2 minTile, uint8_t rotation) const {
    // Ground, trees and buildings as for any building; the farm itself is still lifted, so no
    // farm check (NeedsFarm comes only once everything else is fine)
    PlacementContext context = m_Simulation.MakePlacementContext(m_World);
    context.objects = nullptr;
    const BuildingComponent& building = m_Simulation.Objects().Building(id);
    PlacementCheck check = ValidatePlacement(context, building.type, rotation, minTile);
    return (check.error == PlacementError::None || check.error == PlacementError::NeedsFarm) && check.island == building.island;
}

void BuildTool::DestroyLifted(GameObjectId id) {
    GameObjectRegistry& objects = m_Simulation.Objects();
    const BuildingComponent& building = objects.Building(id);
    m_Simulation.Coins().Refund(building.type, building.island, m_Simulation.Economy());
    objects.Destroy(id);
    m_Simulation.MarkBuildingsChanged();
}

void BuildTool::EndMove(bool toPreview) {
    GameObjectId id = m_Moving;
    m_ClickMove = false;
    if (!m_Simulation.Objects().IsAlive(id)) {
        m_Moving = INVALID_GAME_OBJECT;
        return;
    }
    m_Simulation.PlaceLiftedBuilding(id, toPreview ? m_MoveTile : m_MoveFrom, toPreview ? m_Rotation : m_MoveFromRotation);
    BuildingComponent& building = m_Simulation.Objects().Building(id);
    if (BUILDING_TYPES[building.type].role == BuildingRole::Module) {
        const VoxelAnchorComponent& anchor = m_Simulation.Objects().Anchor(id);
        glm::ivec2 minTile(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
        building.owner = FindModuleFarm(m_Simulation.Objects(), building.type, building.island, minTile, anchor.footprint / TILE_SIZE, m_MoveOwner);
    }
    StampLook(id);

    // The farm's modules: set down around it (or back where they stood); blocked ones are destroyed
    for (const Carried& carried : m_Carried) {
        if (!m_Simulation.Objects().IsAlive(carried.id)) continue; // CarriedTarget needs m_Moving: cleared after
        glm::ivec2 tile = carried.minTile;
        uint8_t rotation = carried.rotation;
        if (toPreview) {
            CarriedTarget(carried, m_MoveTile, m_Rotation, tile, rotation);
            if (!CarriedFits(carried.id, tile, rotation)) {
                DestroyLifted(carried.id);
                continue;
            }
        }
        m_Simulation.PlaceLiftedBuilding(carried.id, tile, rotation);
        StampLook(carried.id);
    }
    if (!m_Carried.empty()) m_LocationRevision++; // The carried tiles go away
    m_Moving = INVALID_GAME_OBJECT;
    m_Carried.clear();
    m_CarriedTiles.clear();
    m_CarriedValid.clear();
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

void BuildTool::ClearLook(GameObjectId id) {
    std::erase_if(m_Constructions, [id](const Construction& construction) { return construction.id == id; }); // Moved or demolished
    ClearBox(m_Simulation.Objects().Anchor(id), BUILDING_TYPES[m_Simulation.Objects().Building(id).type]);
}

void BuildTool::ClearBox(const VoxelAnchorComponent& anchor, const BuildingType& building) {
    m_Editor.FillBox(anchor.origin, glm::ivec3(anchor.footprint.x, BuildingHeight(building), anchor.footprint.y), Block::AIR);
    if (building.belowGround > 0) {
        // Pilings and hulls stood in the ground or the sea: put the generated terrain back
        RestoreTerrain(anchor.origin - glm::ivec3(0, building.belowGround, 0), glm::ivec3(anchor.footprint.x, building.belowGround, anchor.footprint.y));
    }
}

// Clears the building's voxels (the ground under it was never changed) and frees its tiles
void BuildTool::Demolish(GameObjectId id) {
    GameObjectRegistry& objects = m_Simulation.Objects();
    if (!objects.IsAlive(id)) return;

    const VoxelAnchorComponent anchor = objects.Anchor(id);
    const BuildingComponent component = objects.Building(id);
    const BuildingType& building = BUILDING_TYPES[component.type];
    // It comes down in a heap of rubble, unless it was still going up (then it just goes)
    bool goingUp = std::any_of(m_Constructions.begin(), m_Constructions.end(), [id](const Construction& c) { return c.id == id; });
    if (!goingUp && m_Demolitions.size() < (size_t)MAX_DEMOLITIONS) {
        m_Demolitions.push_back({ anchor, component.type, component.variant, component.rotation, 0.0f }); // Within the reserve
    } else {
        ClearLook(id);
    }
    // A farm's modules go with it
    if (ModuleTypeOf(component.type) >= 0) {
        for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
            GameObjectId module = objects.IdAtSlot(slot);
            if (module != INVALID_GAME_OBJECT && objects.Building(module).owner == id) Demolish(module);
        }
    }

    glm::ivec2 minTile(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
    m_Simulation.Occupancy().Release(minTile, anchor.footprint / TILE_SIZE, id);
    m_Simulation.Coins().Refund(component.type, component.island, m_Simulation.Economy());
    if (building.role == BuildingRole::Storage) m_Simulation.Economy().OnWarehouseRemoved(component.island);
    objects.Destroy(id);
    m_Simulation.MarkBuildingsChanged();
}
