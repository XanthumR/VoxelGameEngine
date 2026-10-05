#pragma once

#include "Core/LaunchOptions.h"
#include "Gameplay/DebugEditTool.h"
#include "Gameplay/FreeFlyCamera.h"
#include "Gameplay/Picking.h"
#include "Gameplay/StrategyCamera.h"
#include "Rendering/GpuChunkCache.h"
#include "Rendering/GrassAnimator.h"
#include "Rendering/OceanSimulation.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/ShoreMap.h"
#include "Rendering/VoxelRenderer.h"
#include "Simulation/GameClock.h"
#include "Simulation/Simulation.h"
#include "UI/DebugOverlay.h"
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
    void RunFrame(double frameStartTime, double frameSeconds, float deltaTime);
    void HandleKeys(float deltaTime);
    void UpdatePicking();
    void SetCameraMode(CameraMode mode);
    ICamera& ActiveCamera();
    void Shutdown();

    static void OnMouseMove(GLFWwindow* window, double x, double y);
    static void OnScroll(GLFWwindow* window, double xOffset, double yOffset);

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

    // Game simulation (fixed 10 Hz steps)
    GameClock m_Clock;
    Simulation m_Simulation;

    // Rendering
    RenderSettings m_Settings;
    VoxelRenderer m_Renderer;
    OceanSimulation m_Ocean;
    ShoreMap m_Shore;
    GrassAnimator m_Grass;
    DebugOverlay m_Overlay;

    // Edge state for the hotkeys
    bool m_TabWasPressed = false, m_F1WasPressed = false, m_F3WasPressed = false;
    bool m_CWasPressed = false, m_LWasPressed = false;
    bool m_PageUpWasPressed = false, m_PageDownWasPressed = false;
};
