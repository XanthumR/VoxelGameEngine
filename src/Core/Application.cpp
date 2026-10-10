#include "Core/Application.h"

#include "Core/Screenshot.h"
#include "Economy/ProductionChains.h"
#include "Simulation/BuildingTypes.h"
#include "UI/WorldMarkers.h"
#include "Gameplay/FigureModels.h"
#include "UI/Hud.h"
#include "World/BlockTypes.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {

const char* TREE_MODEL_PATH = "assets/tree.vox";
const char* BUILDING_MODEL_DIRECTORY = "assets/buildings";
const float SMOKE_DISTANCE = 1500.0f; // Voxels from the camera's focus within which chimneys smoke
const float ANIMATION_DISTANCE = 700.0f; // ...and within which buildings' moving parts are drawn
const glm::ivec2 SPAWN_SEARCH_START(640, 640); // Spawn is the nearest decent island to here

} // namespace

Application::Application(const LaunchOptions& options)
    : m_Options(options),
      m_Streamer(m_World, m_Cache, m_Trees),
      m_Editor(m_World, m_Streamer),
      m_Terrain(m_Trees),
      m_EditTool(m_World, m_Editor),
      m_Simulation(m_Terrain),
      m_RoadTool(m_World, m_Editor, m_Simulation),
      m_BuildTool(m_World, m_Editor, m_Simulation, m_RoadTool, m_BuildingModels, m_Terrain) {
    m_Settings.renderScale = options.renderScale;
}

Application::~Application() {
    Shutdown();
}

void Application::OnMouseMove(GLFWwindow* window, double x, double y) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->m_Ui.OnCursorPos(x, y);
    bool captured = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    app->m_FreeFlyCamera.OnMouseMove(x, y, app->m_CameraMode == CameraMode::FreeFly && captured);
}

void Application::OnScroll(GLFWwindow* window, double /*xOffset*/, double yOffset) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    // ImGui chains this callback; ignore the wheel while it scrolls a UI window
    bool usedByUi = app->m_Ui.OnScroll(yOffset);
    bool uiWantsMouse = usedByUi || (app->m_ImGuiReady && !app->MouseFree());
    // Shift + wheel turns the building being placed or moved; otherwise the wheel zooms
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    bool rotated = app->m_CameraMode == CameraMode::Strategy && !uiWantsMouse && shift && app->m_BuildTool.Rotate(yOffset > 0 ? 1 : -1);
    if (app->m_CameraMode == CameraMode::Strategy && !uiWantsMouse && !rotated) app->m_StrategyCamera.OnScroll(yOffset);
}

void Application::OnMouseButton(GLFWwindow* window, int button, int action, int mods) {
    static_cast<Application*>(glfwGetWindowUserPointer(window))->m_Ui.OnMouseButton(button, action, mods);
}

void Application::OnKey(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    static_cast<Application*>(glfwGetWindowUserPointer(window))->m_Ui.OnKey(key, action, mods);
}

void Application::OnChar(GLFWwindow* window, unsigned int codepoint) {
    static_cast<Application*>(glfwGetWindowUserPointer(window))->m_Ui.OnChar(codepoint);
}

bool Application::MouseFree() const {
    return !ImGui::GetIO().WantCaptureMouse && !m_Ui.WantsMouse();
}

ICamera& Application::ActiveCamera() {
    if (m_CameraMode == CameraMode::FreeFly) return m_FreeFlyCamera;
    return m_StrategyCamera;
}

// The new camera starts where the old one was looking
void Application::SetCameraMode(CameraMode mode) {
    if (mode == CameraMode::FreeFly && m_CameraMode == CameraMode::Strategy) {
        m_FreeFlyCamera.SetPosition(m_StrategyCamera.Position());
        m_FreeFlyCamera.SetLook(m_StrategyCamera.Yaw(), -m_StrategyCamera.Pitch());
    } else if (mode == CameraMode::Strategy && m_CameraMode == CameraMode::FreeFly) {
        glm::vec3 position = m_FreeFlyCamera.Position();
        m_StrategyCamera.SetYaw(m_FreeFlyCamera.Yaw());
        m_StrategyCamera.SetTarget(glm::vec3(position.x, (float)(SEA_LEVEL + ISLAND_HEIGHT) / VOXELS_PER_UNIT, position.z));
    }
    m_CameraMode = mode;
    // Free-fly captures the mouse for looking around; the strategy camera needs the cursor
    glfwSetInputMode(m_Window, GLFW_CURSOR, mode == CameraMode::FreeFly ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

bool Application::Init() {
    // Window and OpenGL 4.4 core context
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    m_Window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL Compute DDA Voxel", NULL, NULL);
    if (!m_Window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        return false;
    }
    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(m_Options.vsync ? 1 : 0);
    glfwSetWindowUserPointer(m_Window, this);
    // Installed before ImGui, which chains them
    glfwSetCursorPosCallback(m_Window, OnMouseMove);
    glfwSetScrollCallback(m_Window, OnScroll);
    glfwSetMouseButtonCallback(m_Window, OnMouseButton);
    glfwSetKeyCallback(m_Window, OnKey);
    glfwSetCharCallback(m_Window, OnChar);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    m_GpuName = (const char*)glGetString(GL_RENDERER);
    std::cout << "GPU: " << m_GpuName << " (OpenGL " << (const char*)glGetString(GL_VERSION) << ")" << std::endl;
    m_GpuTimers.Init();

    // GPU systems (each loads its shaders, so a missing or broken file fails fast)
    if (!m_Renderer.Init() || !m_Ocean.Init() || !m_Shore.Init() || !m_Grass.Init(m_Terrain) || !m_Cache.Init() ||
        !m_FigureRenderer.Init()) {
        return false;
    }
    // Voxel object models: boats, people (every look and walk frame), carts (every trot frame and load)
    int mooredBoat = m_Renderer.AddObjectModel(FishingBoats::BuildModel(false));
    m_Boats.SetModels(mooredBoat, m_Renderer.AddObjectModel(FishingBoats::BuildModel(true)));
    int personBase = -1;
    for (int tier = 0; tier < PERSON_TIERS; tier++) {
        for (int variant = 0; variant < PERSON_VARIANTS; variant++) {
            for (int frame = 0; frame < WALK_FRAMES; frame++) {
                int index = m_Renderer.AddObjectModel(BuildPersonModel(tier, variant, frame));
                if (personBase < 0) personBase = index;
            }
        }
    }
    m_Walkers.SetModelBase(personBase);
    int anchoredShip = m_Renderer.AddObjectModel(ShipControl::BuildModel(false));
    m_ShipControl.SetModels(anchoredShip, m_Renderer.AddObjectModel(ShipControl::BuildModel(true)));
    m_CartModelBase = -1;
    for (int frame = 0; frame < CART_FRAMES; frame++) {
        for (int load = 0; load < CART_LOADS; load++) {
            bool piled = load > 4;
            int amount = load == 0 ? 0 : (load - 1) % 4 + 1;
            int index = m_Renderer.AddObjectModel(BuildCartModel(frame, piled, amount));
            if (m_CartModelBase < 0) m_CartModelBase = index;
        }
    }

    // Buildings' moving parts
    std::vector<VoxelObjectModel> partModels;
    m_Animations.Load("assets/buildings/parts", partModels);
    m_Animations.SetAlwaysInUse(m_Options.showcase);
    m_Smoke.SetAlwaysInUse(m_Options.showcase);
    m_PartModelBase = -1;
    for (const VoxelObjectModel& model : partModels) {
        int index = m_Renderer.AddObjectModel(model);
        if (m_PartModelBase < 0) m_PartModelBase = index;
    }

    // The game UI (RmlUi); Dear ImGui stays for the debug windows
    if (!m_Ui.Init(m_Window) || !m_TopBar.Init(m_Ui.Context()) || !m_BuildMenu.Init(m_Ui.Context()) || !m_IslandPanel.Init(m_Ui.Context()) ||
        !m_BuildingInfo.Init(m_Ui.Context()) || !m_ShipPanel.Init(m_Ui.Context()) || !m_TradeRoutes.Init(m_Ui.Context())) {
        std::cout << "Failed to initialize the game UI (RmlUi)" << std::endl;
        return false;
    }

    // Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.85f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.4f, 0.8f, 0.6f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.5f, 1.0f, 0.8f);
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 440");
    m_ImGuiReady = true;

    m_Overlay.Init();
    m_TileOverlay.Init(TILE_SIZE);
    m_ReachScratch.reserve(8192);
    m_Figures.reserve(FigureRenderer::MAX_FIGURES);
    m_VoxelObjects.reserve(4 * VoxelRenderer::MAX_OBJECTS); // Before culling: the renderer draws up to MAX_OBJECTS

    // The tree model must be loaded before the workers start generating
    if (!m_Trees.Load(TREE_MODEL_PATH)) {
        std::cerr << "Tree model missing or empty; terrain will have no trees." << std::endl;
    }
    std::cout << "Loaded " << m_BuildingModels.LoadAll(BUILDING_MODEL_DIRECTORY) << " building models" << std::endl;
    // Felled trees stay felled in every chunk generated from now on
    m_Terrain.SetFelledTrees(&m_Simulation.Trees().Felled());
    m_Streamer.SetFelledTrees(&m_Simulation.Trees().Felled());
    m_Streamer.Start(m_Options.renderDistance);
    return true;
}

// Streams the world around the spawn island, waiting only for the spawn column itself
void Application::Spawn() {
    glm::ivec2 spawnColumn = m_Terrain.FindSpawnColumn(SPAWN_SEARCH_START);
    glm::ivec3 spawnChunk(spawnColumn.x >> 5, 0, spawnColumn.y >> 5);
    m_Streamer.SetPlayerChunk(spawnChunk);
    m_Streamer.UpdateWindows(); // Requests are nearest-first, so the spawn column arrives first
    m_Streamer.WaitForColumn(spawnChunk.x, spawnChunk.z);

    // Free-fly camera 3 voxels above the highest solid block; the strategy camera looks at the
    // spawn point on the ground
    int spawnY = WORLD_HEIGHT - 1;
    while (spawnY > 0 && !IsSolidBlock(m_World.GetVoxel(spawnColumn.x, spawnY, spawnColumn.y))) spawnY--;
    m_FreeFlyCamera.SetPosition(glm::vec3((float)spawnColumn.x, (float)(spawnY + 3), (float)spawnColumn.y) / VOXELS_PER_UNIT);
    m_StrategyCamera.SetTarget(glm::vec3((float)spawnColumn.x, (float)(SEA_LEVEL + ISLAND_HEIGHT), (float)spawnColumn.y) / VOXELS_PER_UNIT);

    // A locked camera (regression screenshots) uses the free-fly camera
    SetCameraMode(m_Options.lockCamera ? CameraMode::FreeFly : CameraMode::Strategy);
}

// --showcase: every building type on the spawn island, each at the nearest free spot of a grid
// around the spawn (the warehouse first, so the island is settled), each farm with all its
// modules. Placed like the player places them, but free; then 100000 coins to try things with.
void Application::PlaceShowcase() {
    glm::ivec2 spawn = m_Terrain.FindSpawnColumn(SPAWN_SEARCH_START);
    glm::ivec2 center(ColumnToTile(spawn.x), ColumnToTile(spawn.y));
    constexpr int SPACING = 5, MAX_RING = 16; // Grid tiles
    int placed = 0;
    for (uint16_t type = 0; type < BUILDING_TYPES.size(); type++) {
        if (BUILDING_TYPES[type].role == BuildingRole::Module) continue; // With their farm below
        GameObjectId id = INVALID_GAME_OBJECT;
        for (int ring = 0; ring <= MAX_RING && id == INVALID_GAME_OBJECT; ring++) {
            for (int dz = -ring; dz <= ring && id == INVALID_GAME_OBJECT; dz++) {
                for (int dx = -ring; dx <= ring && id == INVALID_GAME_OBJECT; dx++) {
                    if (std::max(std::abs(dx), std::abs(dz)) == ring) id = m_BuildTool.Place(type, 0, center + glm::ivec2(dx, dz) * SPACING);
                }
            }
        }
        if (id == INVALID_GAME_OBJECT) {
            std::cout << "Showcase: no room for a " << BUILDING_TYPES[type].name << std::endl;
            continue;
        }
        placed++;

        // A farm's modules, on the free tiles in its range
        int module = ModuleTypeOf(type);
        if (module < 0) continue;
        const ProductionChain& chain = PRODUCTION_CHAINS[BUILDING_TYPES[type].chain];
        const VoxelAnchorComponent& farm = m_Simulation.Objects().Anchor(id);
        glm::ivec2 farmMin(ColumnToTile(farm.origin.x), ColumnToTile(farm.origin.z));
        glm::ivec2 farmMax = farmMin + farm.footprint / TILE_SIZE + (int)chain.radius;
        int modules = 0;
        m_BuildTool.SelectModules(id); // Its modules go to this farm
        for (int z = farmMin.y - chain.radius; z < farmMax.y && modules < chain.fullSpeedCount; z++) {
            for (int x = farmMin.x - chain.radius; x < farmMax.x && modules < chain.fullSpeedCount; x++) {
                if (m_BuildTool.Place((uint16_t)module, 0, glm::ivec2(x, z)) != INVALID_GAME_OBJECT) modules++;
            }
        }
        m_BuildTool.SelectType(BuildTool::NO_TYPE);
        placed += modules;
    }
    m_Simulation.Coins().SetCoins(100000); // To try things with
    m_BuildTool.UnlockAllTiers();
    std::cout << "Showcase: placed " << placed << " buildings" << std::endl;
}

int Application::Run() {
    if (!Init()) return -1;
    Spawn();

    double lastTime = glfwGetTime();
    double showcaseTime = m_Options.showcase ? lastTime + 1.5 : -1.0; // Once the island around the spawn has streamed in
    while (!glfwWindowShouldClose(m_Window)) {
        if (showcaseTime > 0.0 && glfwGetTime() >= showcaseTime) {
            PlaceShowcase();
            showcaseTime = -1.0;
        }
        double frameStartTime = glfwGetTime();
        // The simulation gets the real elapsed time (GameClock caps it); cameras get a clamped
        // one so a stall (window drag, breakpoint) can't launch the player through walls
        double frameSeconds = frameStartTime - lastTime;
        float deltaTime = (float)std::min(frameSeconds, 0.05);
        lastTime = frameStartTime;
        RunFrame(frameStartTime, frameSeconds, deltaTime);
    }
    return 0;
}

bool Application::KeyPressed(int key, bool& wasPressed) {
    bool pressed = glfwGetKey(m_Window, key) == GLFW_PRESS;
    bool edge = pressed && !wasPressed;
    wasPressed = pressed;
    return edge;
}

void Application::HandleKeys(float deltaTime) {
    // Escape puts a moved building back or drops the build selection or tool first; with nothing
    // selected it quits
    if (KeyPressed(GLFW_KEY_ESCAPE, m_EscapeWasPressed)) {
        if (m_CameraMode == CameraMode::Strategy && m_BuildTool.Cancel()) {
        } else if (m_CameraMode == CameraMode::Strategy && m_ShipControl.Selected() != INVALID_SHIP) {
            m_ShipControl.Deselect();
        } else if (m_CameraMode == CameraMode::Strategy && m_BuildTool.InspectedBuilding() != INVALID_GAME_OBJECT) {
            m_BuildTool.ClearInspection();
        } else {
            glfwSetWindowShouldClose(m_Window, true);
        }
    }

    // F1 switches between the strategy camera and the free-fly debug camera
    if (KeyPressed(GLFW_KEY_F1, m_F1WasPressed)) {
        SetCameraMode(m_CameraMode == CameraMode::Strategy ? CameraMode::FreeFly : CameraMode::Strategy);
    }

    // Free-fly: Tab frees the mouse for the UI, or captures it again
    if (KeyPressed(GLFW_KEY_TAB, m_TabWasPressed) && m_CameraMode == CameraMode::FreeFly) {
        bool captured = glfwGetInputMode(m_Window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
        glfwSetInputMode(m_Window, GLFW_CURSOR, captured ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    if (KeyPressed(GLFW_KEY_F3, m_F3WasPressed)) m_Overlay.Visible() = !m_Overlay.Visible();
    // Game speed: P or Space pauses, + and - step through 1x, 2x, 4x
    bool plus = glfwGetKey(m_Window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(m_Window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
    bool minus = glfwGetKey(m_Window, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(m_Window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
    bool pausePressed = KeyPressed(GLFW_KEY_P, m_PWasPressed);
    pausePressed |= KeyPressed(GLFW_KEY_SPACE, m_SpaceWasPressed) && m_CameraMode == CameraMode::Strategy; // Space flies up in free-fly
    if (pausePressed) {
        if (m_GameSpeed > 0) m_SpeedBeforePause = m_GameSpeed;
        m_GameSpeed = m_GameSpeed > 0 ? 0 : m_SpeedBeforePause;
    }
    if (plus && !m_PlusWasPressed) m_GameSpeed = m_GameSpeed == 0 ? 1 : std::min(4, m_GameSpeed * 2);
    if (minus && !m_MinusWasPressed) m_GameSpeed = m_GameSpeed <= 1 ? 1 : m_GameSpeed / 2;
    m_PlusWasPressed = plus;
    m_MinusWasPressed = minus;
    // Debug views on F-keys (the letters are the build tools')
    if (KeyPressed(GLFW_KEY_F5, m_F5WasPressed)) m_Settings.chunkViewer = !m_Settings.chunkViewer;
    if (KeyPressed(GLFW_KEY_F6, m_F6WasPressed)) m_Settings.lightVisualizer = !m_Settings.lightVisualizer;

    if (m_CameraMode == CameraMode::FreeFly) m_EditTool.HandleBlockSelectKeys(m_Window);

    // Render distance, one chunk per press
    if (KeyPressed(GLFW_KEY_PAGE_UP, m_PageUpWasPressed)) m_Streamer.SetRenderDistance(m_Streamer.RenderDistance() + 1);
    if (KeyPressed(GLFW_KEY_PAGE_DOWN, m_PageDownWasPressed)) m_Streamer.SetRenderDistance(m_Streamer.RenderDistance() - 1);

    ImGuiIO& io = ImGui::GetIO();
    if (m_CameraMode == CameraMode::FreeFly) {
        m_FreeFlyCamera.UpdateMovement(m_Window, deltaTime, m_World);
    } else {
        m_StrategyCamera.Update(m_Window, deltaTime, MouseFree(), !io.WantCaptureKeyboard);
    }
}

// Strategy camera: the voxel under the mouse cursor. Free-fly: the voxel under the crosshair.
void Application::UpdatePicking() {
    m_Hover = PickResult();
    m_HoverIsland = NO_ISLAND;
    // The island bar shows the island at the camera's focus, as in Anno (the last one over open sea)
    glm::vec3 focusVoxel = ActiveCamera().FocusPoint() * VOXELS_PER_UNIT;
    IslandId focusIsland = m_Simulation.Islands().IslandIdAt((int)std::floor(focusVoxel.x), (int)std::floor(focusVoxel.z));
    if (focusIsland != NO_ISLAND) m_PanelIsland = focusIsland;
    if (m_CameraMode == CameraMode::Strategy) {
        if (!MouseFree()) return; // Pointing at a UI window
        double cursorX, cursorY;
        glfwGetCursorPos(m_Window, &cursorX, &cursorY);
        int width, height;
        glfwGetWindowSize(m_Window, &width, &height); // Cursor coordinates are window coordinates
        m_Hover = PickUnderCursor(m_StrategyCamera, glm::vec2((float)cursorX, (float)cursorY), glm::ivec2(width, height), m_World);
    } else {
        int width, height;
        glfwGetWindowSize(m_Window, &width, &height);
        m_Hover = PickUnderCursor(m_FreeFlyCamera, glm::vec2(width * 0.5f, height * 0.5f), glm::ivec2(width, height), m_World);
    }
    if (m_Hover.hit) m_HoverIsland = m_Simulation.Islands().IslandIdAt(m_Hover.voxel.x, m_Hover.voxel.z);
}

// Rebuilds the per-tile ground highlights when anything they show changed: road range colors
// while building (warehouse reach; for houses the marketplaces' reach, for service buildings the
// reach of their type), the reach of a hovered or previewed warehouse or service building, and the
// road tool's path
void Application::UpdateTileOverlay() {
    glm::ivec3 focus = glm::ivec3(glm::floor(m_StrategyCamera.FocusPoint() * VOXELS_PER_UNIT));
    bool moved = m_TileOverlay.Recenter(glm::ivec2(ColumnToTile(focus.x), ColumnToTile(focus.z)));

    const GameObjectRegistry& objects = m_Simulation.Objects();
    RoadNetwork& roads = m_Simulation.Roads();
    TileOverlayKey key;
    key.roadRevision = roads.Revision();
    key.logisticsRevision = m_Simulation.Logistics().Revision();
    key.roadPreviewRevision = m_RoadTool.PreviewRevision();
    key.selection = m_BuildTool.SelectedType();
    GameObjectId hovered = m_BuildTool.HoveredBuilding();
    if (objects.IsAlive(hovered)) {
        BuildingRole role = BUILDING_TYPES[objects.Building(hovered).type].role;
        if (role == BuildingRole::Storage) key.highlightedWarehouse = hovered;
        if (role == BuildingRole::Service) key.highlightedService = hovered;
    }
    bool reachPreview = key.selection >= 0 &&
        (BUILDING_TYPES[key.selection].role == BuildingRole::Storage || BUILDING_TYPES[key.selection].role == BuildingRole::Service) &&
        m_BuildTool.HasPlacementPreview();
    if (reachPreview) {
        key.previewReachType = key.selection;
        key.previewMinTile = m_BuildTool.PreviewMinTile();
        key.previewTiles = m_BuildTool.PreviewTiles();
    }
    key.locationRevision = m_BuildTool.LocationRevision();
    if (!moved && key == m_TileOverlayKey) return;
    m_TileOverlayKey = key;

    m_TileOverlay.Clear();
    bool anyHighlight = key.highlightedWarehouse != INVALID_GAME_OBJECT || key.highlightedService != INVALID_GAME_OBJECT;
    if (key.selection != BuildTool::NO_TYPE || anyHighlight) {
        // Houses care about marketplace reach, service buildings about the reach of their type,
        // everything else about warehouse reach
        ServiceType service = ServiceType::Count;
        if (key.selection >= 0 && BUILDING_TYPES[key.selection].role == BuildingRole::Residence) service = ServiceType::Marketplace;
        if (key.selection >= 0 && BUILDING_TYPES[key.selection].role == BuildingRole::Service) service = BUILDING_TYPES[key.selection].service;
        if (key.highlightedService != INVALID_GAME_OBJECT) service = BUILDING_TYPES[objects.Building(key.highlightedService).type].service;
        roads.ForEach([&](glm::ivec2 tile, const RoadTile& road) {
            uint8_t color;
            if (service != ServiceType::Count) {
                size_t s = (size_t)service;
                color = road.serviceDistance[s] <= SERVICE_ROAD_RANGE[s] ? TileOverlay::MARKET_IN_RANGE : TileOverlay::ROAD_OUT_OF_RANGE;
                if (key.highlightedService != INVALID_GAME_OBJECT && road.service[s] == key.highlightedService) color = TileOverlay::MARKET_REACH;
            } else {
                color = road.distance <= WAREHOUSE_ROAD_RANGE ? TileOverlay::ROAD_IN_RANGE : TileOverlay::ROAD_OUT_OF_RANGE;
            }
            if (key.highlightedWarehouse != INVALID_GAME_OBJECT && road.warehouse == key.highlightedWarehouse) color = TileOverlay::WAREHOUSE_REACH;
            m_TileOverlay.Set(tile, color);
        });
    }
    if (key.previewReachType >= 0) {
        const BuildingType& previewed = BUILDING_TYPES[key.previewReachType];
        bool service = previewed.role == BuildingRole::Service;
        m_Simulation.Logistics().PreviewReach(roads, key.previewMinTile, key.previewTiles,
            service ? SERVICE_ROAD_RANGE[(size_t)previewed.service] : WAREHOUSE_ROAD_RANGE, m_ReachScratch);
        for (const glm::ivec2& tile : m_ReachScratch) m_TileOverlay.Set(tile, service ? TileOverlay::MARKET_REACH : TileOverlay::WAREHOUSE_REACH);
    }
    if (m_BuildTool.HasLocationPreview()) {
        for (const glm::ivec2& tile : m_BuildTool.PreviewLocationTiles()) m_TileOverlay.Set(tile, TileOverlay::LOCATION_COUNTED);
    }
    // A moved farm's modules: green where they fit, red where they would be destroyed
    for (size_t i = 0; i < m_BuildTool.CarriedTiles().size(); i++) {
        m_TileOverlay.Set(m_BuildTool.CarriedTiles()[i], m_BuildTool.CarriedValid()[i] ? TileOverlay::PREVIEW_ADD : TileOverlay::PREVIEW_INVALID);
    }
    if (key.selection == BuildTool::ROAD) {
        const std::vector<glm::ivec2>& path = m_RoadTool.PathTiles();
        const std::vector<uint8_t>& valid = m_RoadTool.PathValid();
        for (size_t i = 0; i < path.size(); i++) {
            if (m_RoadTool.Removing() && !valid[i]) continue; // Removing: only the road tiles matter
            uint8_t color = !valid[i] ? TileOverlay::PREVIEW_INVALID : (m_RoadTool.Removing() ? TileOverlay::PREVIEW_REMOVE : TileOverlay::PREVIEW_ADD);
            m_TileOverlay.Set(path[i], color);
        }
    }
    m_TileOverlay.Upload();
}

// A felled tree's voxels go, a regrown one is stamped back: edited directly where the CPU has the
// chunk, regenerated by the workers where only the GPU has it
void Application::ApplyTreeChange(const TreeChange& change) {
    glm::ivec3 base(change.root.x, m_Terrain.TreeRootY(change.root.x, change.root.y), change.root.y);
    m_Editor.StampModel(base, m_Trees, change.felled, Block::GRASS); // Trees stand on grass
    glm::ivec3 minVoxel = base + m_Trees.min, maxVoxel = base + m_Trees.max;
    for (int cy = std::max(0, minVoxel.y >> 5); cy <= std::min(CHUNK_LAYERS - 1, maxVoxel.y >> 5); cy++) {
        for (int cz = minVoxel.z >> 5; cz <= maxVoxel.z >> 5; cz++) {
            for (int cx = minVoxel.x >> 5; cx <= maxVoxel.x >> 5; cx++) m_Streamer.RegenerateChunk(cx, cy, cz);
        }
    }
}

void Application::RunFrame(double frameStartTime, double frameSeconds, float deltaTime) {
    // Window size and render targets
    glfwGetFramebufferSize(m_Window, &m_WindowWidth, &m_WindowHeight);
    bool minimized = (m_WindowWidth == 0 || m_WindowHeight == 0);
    if (!minimized) m_Renderer.ResizeTargets(m_WindowWidth, m_WindowHeight, m_Settings.renderScale);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // Game UI clicks (from last frame's events)
    if (int speed = m_TopBar.TakeSpeedRequest(); speed >= 0) m_GameSpeed = speed;
    if (m_TopBar.TakeRoutesToggle()) m_RoutesOpen = !m_RoutesOpen;
    m_BuildMenu.ApplyRequests(m_BuildTool);
    m_BuildingInfo.ApplyRequests(m_Simulation, m_BuildTool);
    m_ShipPanel.ApplyRequests(m_Simulation, m_ShipControl);
    if (!m_TradeRoutes.ApplyRequests(m_Simulation, m_ShipControl.Selected())) m_RoutesOpen = false;

    // --- World streaming ---
    glm::ivec3 focusChunk = glm::ivec3(glm::floor(ActiveCamera().FocusPoint() * VOXELS_PER_UNIT / (float)CHUNK_SIZE));
    bool movedChunk = m_Streamer.SetPlayerChunk(focusChunk);
    m_Streamer.IntegrateResults(frameStartTime, STREAMING_BUDGET_SECONDS);
    if (movedChunk) m_Streamer.UpdateWindows();
    if (movedChunk || m_Editor.ConsumeTerrainChanged()) m_Grass.MarkDirty();
    m_Grass.Update(glfwGetTime(), m_Streamer.PlayerChunk(), m_World);

    // --- Input and gameplay ---
    HandleKeys(deltaTime);
    m_Overlay.UpdateMinimap(m_World, ActiveCamera().FocusPoint());
    if (m_CameraMode == CameraMode::FreeFly) m_EditTool.HandleMouse(m_Window, m_FreeFlyCamera);
    UpdatePicking();
    if (m_CameraMode == CameraMode::Strategy) {
        ImGuiIO& io = ImGui::GetIO();
        // Ships first: while one is selected, clicks are its orders, not building ones
        m_ShipControl.Update(m_Window, m_Hover, MouseFree(), m_BuildTool.SelectedType() != BuildTool::NO_TYPE, m_Simulation);
        m_BuildTool.Update(m_Window, m_Hover, MouseFree() && m_ShipControl.Selected() == INVALID_SHIP, !io.WantCaptureKeyboard);
        UpdateTileOverlay();
    }

    // --- Game simulation: fixed steps, independent of the frame rate ---
    int steps = m_Clock.StepsToRun(frameSeconds, m_GameSpeed);
    for (int i = 0; i < steps; i++) m_Simulation.FixedUpdate((float)GameClock::TICK_SECONDS);
    // Houses that upgraded or downgraded get their new look
    for (GameObjectId id : m_Simulation.Population().LookChanges()) m_BuildTool.RefreshLook(id);
    m_Simulation.Population().ClearLookChanges();
    // Trees cut down or grown back
    for (const TreeChange& change : m_Simulation.Trees().Changes()) ApplyTreeChange(change);
    m_Simulation.Trees().ClearChanges();
    // Buildings going up and coming down; their dust stops while the game is paused, like the smoke
    m_BuildTool.AnimateBuildings(deltaTime, m_GameSpeed > 0 ? &m_Smoke : nullptr);

    // --- UI ---
    bool strategy = m_CameraMode == CameraMode::Strategy;
    m_TopBar.SetVisible(strategy);
    m_BuildMenu.SetVisible(strategy && m_BuildTool.InspectedBuilding() == INVALID_GAME_OBJECT); // The object menu takes its place
    m_TopBar.Update(m_Simulation.Coins(), m_Simulation.Economy(), m_Simulation.Objects(), m_GameSpeed, m_RoutesOpen);
    double cursorX, cursorY;
    glfwGetCursorPos(m_Window, &cursorX, &cursorY);
    int windowWidth, windowHeight;
    glfwGetWindowSize(m_Window, &windowWidth, &windowHeight);
    // Cursor positions are in window coordinates, the UI in framebuffer pixels
    glm::vec2 cursor((float)cursorX * m_WindowWidth / std::max(1, windowWidth), (float)cursorY * m_WindowHeight / std::max(1, windowHeight));
    m_BuildMenu.Update(m_BuildTool, m_Simulation, cursor, glm::ivec2(m_WindowWidth, m_WindowHeight));
    m_IslandPanel.Update(m_PanelIsland, m_Simulation.Economy(), strategy);
    {
        // The hovered building's tooltip, unless the build tool has a selection or its panel is open
        GameObjectId hovered = m_BuildTool.HoveredBuilding();
        bool tooltip = strategy && m_BuildTool.SelectedType() == BuildTool::NO_TYPE && m_BuildTool.MovingBuilding() == INVALID_GAME_OBJECT &&
                       MouseFree() && hovered != m_BuildTool.InspectedBuilding();
        m_BuildingInfo.Update(tooltip ? hovered : INVALID_GAME_OBJECT, strategy ? m_BuildTool.InspectedBuilding() : INVALID_GAME_OBJECT,
            m_Simulation, cursor, glm::ivec2(m_WindowWidth, m_WindowHeight));
    }
    m_ShipPanel.Update(strategy ? m_ShipControl.Selected() : INVALID_SHIP, m_Simulation, m_ShipControl.LastOrderFailed());
    m_TradeRoutes.Update(strategy && m_RoutesOpen, m_Simulation, m_ShipControl.Selected());
    if (!minimized) m_Ui.Update(m_WindowWidth, m_WindowHeight);
    OverlayContext overlay{ m_Window, deltaTime, ImGui::GetIO().Framerate, m_EditTool, m_Streamer, m_Cache, m_World,
        m_Settings, m_Renderer.Targets(), m_Grass.ActiveCount(), GrassAnimator::ANIMATION_RADIUS,
        m_CameraMode == CameraMode::Strategy ? "Strategy (F1: free-fly)" : "Free-fly (F1: strategy)", m_Hover,
        m_HoverIsland, m_PanelIsland, m_Simulation, m_Clock.DroppedSteps(), m_BuildTool, m_Walkers.Count(), m_GpuTimers, m_CpuFrameMs,
        m_GpuName };
    m_Overlay.Draw(overlay);
    if (m_CameraMode == CameraMode::Strategy) {
        int width, height;
        glfwGetWindowSize(m_Window, &width, &height);
        DrawBuildingMarkers(m_StrategyCamera, m_Simulation.Objects(), glm::ivec2(width, height));
        DrawShipMarkers(m_Simulation.Ships(), m_ShipControl.Selected(), m_Clock.Alpha(), m_StrategyCamera, glm::ivec2(width, height));
    }

    if (minimized) {
        // Nothing to draw into; keep the UI frame balanced and wait for events
        ImGui::Render();
        glfwWaitEvents();
        return;
    }

    if (m_Options.lockCamera) m_FreeFlyCamera.LockView(m_Options.cameraVoxel, m_Options.cameraYaw, m_Options.cameraPitch);

    // --- GPU simulation: ocean waves, grass ---
    m_GpuTimers.BeginFrame();
    float animationTime = m_Options.fixedTime >= 0.0f ? m_Options.fixedTime : (float)glfwGetTime();
    m_GpuTimers.Mark("Ocean waves (FFT)");
    m_Ocean.Update(animationTime);
    m_GpuTimers.Mark("Grass");
    m_Grass.Animate(animationTime, m_Cache);
    // Figures: walkers, chimney smoke and fishing boats, drawn into the voxels on the GPU
    float gameDelta = deltaTime * (float)m_GameSpeed; // Paused or sped up with the simulation
    m_Walkers.Update(gameDelta, m_Simulation.Objects(), m_Simulation.Roads(), m_Simulation.Economy());
    glm::vec3 focus = ActiveCamera().FocusPoint() * VOXELS_PER_UNIT;
    m_Smoke.Update(gameDelta, m_Simulation.Objects(), m_BuildingModels, glm::vec2(focus.x, focus.z), SMOKE_DISTANCE);
    m_Boats.Update(gameDelta, m_Simulation.Objects(), m_BuildingModels, m_Terrain);
    m_Figures.clear();
    m_Smoke.AppendFigures(m_Figures);
    m_VoxelObjects.clear();
    m_Boats.AppendObjects(m_VoxelObjects);
    m_VoxelObjects.insert(m_VoxelObjects.end(), m_Walkers.Objects().begin(), m_Walkers.Objects().end());
    AppendCartObjects(m_Simulation.Objects(), m_Clock.Alpha(), m_CartModelBase, m_VoxelObjects);
    m_ShipControl.AppendObjects(m_Simulation.Ships(), m_Clock.Alpha(), gameDelta, m_VoxelObjects);
    m_Animations.Update(gameDelta, m_Simulation.Objects());
    m_Animations.AppendObjects(m_Simulation.Objects(), glm::vec2(focus.x, focus.z), ANIMATION_DISTANCE, m_PartModelBase, m_VoxelObjects,
        [this](GameObjectId id) { return id == m_BuildTool.MovingBuilding() || m_BuildTool.IsUnderConstruction(id); });
    m_GpuTimers.Mark("Smoke puffs");
    m_FigureRenderer.Draw(m_Figures, m_Cache, SEA_LEVEL);

    // --- Render ---
    const ICamera& camera = ActiveCamera();
    FrameParams frame;
    frame.cameraPos = camera.Position();
    frame.cameraFront = camera.Front();
    frame.cameraUp = camera.Up();
    frame.focusPoint = camera.FocusPoint();
    frame.time = animationTime;
    frame.renderDistance = m_Streamer.RenderDistance();
    frame.windowWidth = m_WindowWidth;
    frame.windowHeight = m_WindowHeight;
    if (m_CameraMode == CameraMode::Strategy) {
        frame.preview = m_BuildTool.Preview();
        frame.overlay = &m_TileOverlay;
    }
    frame.objects = &m_VoxelObjects;
    frame.timers = &m_GpuTimers;
    m_GpuTimers.Mark("Shore map + object setup");
    m_Renderer.Render(frame, m_Settings, m_Cache, m_Ocean, m_Shore);

    if (m_CameraMode == CameraMode::FreeFly) DrawHud(m_EditTool.CrosshairColor(m_FreeFlyCamera), m_EditTool.SelectedBlock());
    // ImGui's background list (the badges over the world) goes under the game UI, its windows (the
    // debug tools) over it
    ImDrawList* background = ImGui::GetBackgroundDrawList();
    ImGui::Render();
    const ImDrawData* imgui = ImGui::GetDrawData();
    m_ImGuiUnderUi = *imgui; // Copies into vectors that keep their capacity
    m_ImGuiOverUi = *imgui;
    m_ImGuiUnderUi.CmdLists.resize(0);
    m_ImGuiOverUi.CmdLists.resize(0);
    for (ImDrawList* list : imgui->CmdLists) (list == background ? m_ImGuiUnderUi : m_ImGuiOverUi).CmdLists.push_back(list);
    m_ImGuiUnderUi.CmdListsCount = m_ImGuiUnderUi.CmdLists.Size;
    m_ImGuiOverUi.CmdListsCount = m_ImGuiOverUi.CmdLists.Size;
    ImGui_ImplOpenGL3_RenderDrawData(&m_ImGuiUnderUi);
    m_GpuTimers.Mark("Game UI (RmlUi)");
    if (m_CameraMode == CameraMode::Strategy) m_Ui.Render(m_WindowWidth, m_WindowHeight);
    m_GpuTimers.Mark("Debug UI (ImGui)");
    ImGui_ImplOpenGL3_RenderDrawData(&m_ImGuiOverUi);
    m_GpuTimers.EndFrame();
    m_CpuFrameMs = m_CpuFrameMs * 0.95f + (float)(glfwGetTime() - frameStartTime) * 1000.0f * 0.05f;

    if (m_Options.screenshotDelay >= 0.0 && glfwGetTime() >= m_Options.screenshotDelay) {
        if (SaveWindowScreenshot(m_Options.screenshotPath, m_WindowWidth, m_WindowHeight)) {
            std::cout << "Saved screenshot: " << m_Options.screenshotPath << std::endl;
        }
        glfwSetWindowShouldClose(m_Window, true);
    }

    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}

void Application::Shutdown() {
    m_Ui.Shutdown();
    if (m_ImGuiReady) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_ImGuiReady = false;
    }
    m_Streamer.Stop();
    if (m_Window) {
        glfwTerminate();
        m_Window = nullptr;
    }
}
