#pragma once

#include "Core/LaunchOptions.h"
#include "imgui.h"
#include "Gameplay/BuildTool.h"
#include "Gameplay/BuildingAnimations.h"
#include "Gameplay/RoadTool.h"
#include "Gameplay/Carts.h"
#include "Gameplay/FishingBoats.h"
#include "Gameplay/ShipControl.h"
#include "Gameplay/Smoke.h"
#include "Gameplay/Walkers.h"
#include "Gameplay/DebugEditTool.h"
#include "Gameplay/FreeFlyCamera.h"
#include "Gameplay/Picking.h"
#include "Gameplay/StrategyCamera.h"
#include "Rendering/GpuChunkCache.h"
#include "Rendering/GpuTimers.h"
#include "Rendering/GrassAnimator.h"
#include "Rendering/OceanSimulation.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/ShoreMap.h"
#include "Rendering/TileOverlay.h"
#include "Rendering/VoxelRenderer.h"
#include "Rendering/FigureRenderer.h"
#include "Simulation/BuildingModels.h"
#include "Simulation/GameClock.h"
#include "Simulation/Simulation.h"
#include "UI/DebugOverlay.h"
#include "UI/BuildMenu.h"
#include "UI/BuildingInfo.h"
#include "UI/IslandPanel.h"
#include "UI/ShipPanel.h"
#include "UI/TradeRoutes.h"
#include "UI/GameUi.h"
#include "UI/TopBar.h"
#include "World/ChunkStreamer.h"
#include "World/TerrainGenerator.h"
#include "World/VoxModel.h"
#include "World/VoxelWorld.h"
#include "World/WorldEditor.h"

struct GLFWwindow;

// Owns every system and runs the game: window and OpenGL setup, startup (spawn), the frame loop
// that updates and draws everything in order, and shutdown.
class Application {
public:
    explicit Application(const LaunchOptions& options);
    ~Application();

    int Run(); // Returns the process exit code

private:
    enum class CameraMode { Strategy, FreeFly };

    bool Init();
    void Spawn();
    void PlaceShowcase();
    void RunFrame(double frameStartTime, double frameSeconds, float deltaTime);
    void HandleKeys(float deltaTime);
    void UpdatePicking();
    void UpdateTileOverlay();
    void ApplyTreeChange(const TreeChange& change);
    void SetCameraMode(CameraMode mode);
    ICamera& ActiveCamera();
    void Shutdown();

    static void OnMouseMove(GLFWwindow* window, double x, double y);
    static void OnScroll(GLFWwindow* window, double xOffset, double yOffset);
    static void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
    static void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void OnChar(GLFWwindow* window, unsigned int codepoint);
    // Neither the game UI nor an ImGui window is under the cursor
    bool MouseFree() const;

    // Edge-triggered key press: true only on the frame the key goes down
    bool KeyPressed(int key, bool& wasPressed);

    static constexpr int WINDOW_WIDTH = 1920;
    static constexpr int WINDOW_HEIGHT = 1080;
    static constexpr double STREAMING_BUDGET_SECONDS = 0.004; // Per frame, for integrating chunks

    LaunchOptions m_Options;
    GLFWwindow* m_Window = nullptr;
    bool m_ImGuiReady = false;
    int m_WindowWidth = 0, m_WindowHeight = 0; // Framebuffer pixels

    // World (declaration order matters: later members reference earlier ones)
    VoxModel m_Trees;
    VoxelWorld m_World;
    GpuChunkCache m_Cache;
    ChunkStreamer m_Streamer;
    WorldEditor m_Editor;
    TerrainGenerator m_Terrain; // Main-thread copy: spawn search, grass placement

    // Cameras and input
    StrategyCamera m_StrategyCamera;
    FreeFlyCamera m_FreeFlyCamera;
    CameraMode m_CameraMode = CameraMode::Strategy;
    DebugEditTool m_EditTool;
    PickResult m_Hover; // What the cursor (or the free-fly crosshair) points at
    IslandId m_HoverIsland = NO_ISLAND;
    IslandId m_PanelIsland = NO_ISLAND; // The island at the camera's focus (the last one): the island bar's

    // Game simulation (fixed 10 Hz steps)
    GameClock m_Clock;
    Simulation m_Simulation;
    BuildingModelLibrary m_BuildingModels; // .vox looks of the buildings (loaded in Init)
    RoadTool m_RoadTool;   // Strategy camera: drag roads
    BuildTool m_BuildTool; // Strategy camera: build selection, place and demolish buildings

    // Rendering
    RenderSettings m_Settings;
    VoxelRenderer m_Renderer;
    OceanSimulation m_Ocean;
    ShoreMap m_Shore;
    GrassAnimator m_Grass;
    WalkerSystem m_Walkers; // Residents walking the roads (visual only)
    SmokeSystem m_Smoke;    // Chimney smoke (visual only)
    ShipControl m_ShipControl; // Selecting ships, their orders and look
    bool m_RoutesOpen = false; // The trade route window
    FishingBoats m_Boats;   // Fishing boats of the fisheries (visual only)
    FigureRenderer m_FigureRenderer;
    GpuTimers m_GpuTimers;
    float m_CpuFrameMs = 0.0f;      // Main thread time per frame, smoothed (without waiting for vsync)
    const char* m_GpuName = "";
    std::vector<Figure> m_Figures; // This frame's smoke puffs, reserved at setup
    std::vector<VoxelObject> m_VoxelObjects; // This frame's walkers, carts and boats, reserved at setup
    int m_CartModelBase = 0; // First cart model (FigureModels)
    BuildingAnimations m_Animations; // Moving parts of buildings (sails, signs, animals)
    ImDrawData m_ImGuiUnderUi, m_ImGuiOverUi; // ImGui's frame split around the game UI
    int m_PartModelBase = 0;         // Their first model
    DebugOverlay m_Overlay;
    GameUi m_Ui;
    TopBar m_TopBar;
    BuildMenu m_BuildMenu;
    IslandPanel m_IslandPanel;
    BuildingInfo m_BuildingInfo;
    ShipPanel m_ShipPanel;
    TradeRoutes m_TradeRoutes;
    TileOverlay m_TileOverlay;

    // What the tile overlay was last built from; rebuilt only when this changes
    struct TileOverlayKey {
        uint32_t roadRevision = 0xFFFFFFFF, logisticsRevision = 0, roadPreviewRevision = 0;
        int selection = 0;
        GameObjectId highlightedWarehouse = INVALID_GAME_OBJECT;
        GameObjectId highlightedService = INVALID_GAME_OBJECT; // A hovered service building (marketplace, school, ...)
        int previewReachType = -1; // Building type whose reach is previewed (warehouse or service building), or -1
        uint32_t locationRevision = 0;
        glm::ivec2 previewMinTile = glm::ivec2(0), previewTiles = glm::ivec2(0);
        bool operator==(const TileOverlayKey&) const = default;
    };
    TileOverlayKey m_TileOverlayKey;
    std::vector<glm::ivec2> m_ReachScratch; // Warehouse placement preview reach, reserved at setup

    // Edge state for the hotkeys
    bool m_EscapeWasPressed = false, m_TabWasPressed = false, m_F1WasPressed = false, m_F3WasPressed = false;
    bool m_F5WasPressed = false, m_F6WasPressed = false, m_SpaceWasPressed = false;
    bool m_PageUpWasPressed = false, m_PageDownWasPressed = false;
    bool m_PWasPressed = false, m_PlusWasPressed = false, m_MinusWasPressed = false;

    int m_GameSpeed = 1;  // 0 = paused, 1, 2, 4
    int m_SpeedBeforePause = 1;
};
