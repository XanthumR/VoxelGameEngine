#include "Core/Application.h"

#include "Core/Screenshot.h"
#include "UI/Hud.h"
#include "World/BlockTypes.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <iostream>

#ifdef _MSC_VER
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "opengl32.lib")
#endif

namespace {

const char* TREE_MODEL_PATH = "assets/tree.vox";
const glm::ivec2 SPAWN_SEARCH_START(640, 640); // Spawn is the nearest decent island to here

} // namespace

Application::Application(const LaunchOptions& options)
    : m_Options(options),
      m_Streamer(m_World, m_Cache, m_Trees),
      m_Editor(m_World, m_Streamer),
      m_Terrain(m_Trees),
      m_EditTool(m_World, m_Editor) {
    m_Settings.renderScale = options.renderScale;
}

Application::~Application() {
    Shutdown();
}

void Application::OnMouseMove(GLFWwindow* window, double x, double y) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->m_Player.OnMouseMove(x, y, glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED);
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
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetCursorPosCallback(m_Window, OnMouseMove);
    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    // GPU systems (each loads its shaders, so a missing or broken file fails fast)
    if (!m_Renderer.Init() || !m_Ocean.Init() || !m_Shore.Init() || !m_Grass.Init(m_Terrain) || !m_Cache.Init()) {
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

    // The tree model must be loaded before the workers start generating
    if (!m_Trees.Load(TREE_MODEL_PATH)) {
        std::cerr << "Tree model missing or empty; terrain will have no trees." << std::endl;
    }
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

    // Camera 3 voxels above the highest solid block
    int spawnY = WORLD_HEIGHT - 1;
    while (spawnY > 0 && !IsSolidBlock(m_World.GetVoxel(spawnColumn.x, spawnY, spawnColumn.y))) spawnY--;
    m_Player.SetPosition(glm::vec3((float)spawnColumn.x, (float)(spawnY + 3), (float)spawnColumn.y) / VOXELS_PER_UNIT);
}

int Application::Run() {
    if (!Init()) return -1;
    Spawn();

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(m_Window)) {
        double frameStartTime = glfwGetTime();
        // Clamp so a stall (window drag, breakpoint) can't launch the player through walls
        float deltaTime = (float)std::min(frameStartTime - lastTime, 0.05);
        lastTime = frameStartTime;
        RunFrame(frameStartTime, deltaTime);
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
    if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(m_Window, true);

    // Tab frees the mouse for the UI, or captures it again
    if (KeyPressed(GLFW_KEY_TAB, m_TabWasPressed)) {
        bool captured = glfwGetInputMode(m_Window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
        glfwSetInputMode(m_Window, GLFW_CURSOR, captured ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    if (KeyPressed(GLFW_KEY_F3, m_F3WasPressed)) m_Overlay.Visible() = !m_Overlay.Visible();
    if (KeyPressed(GLFW_KEY_C, m_CWasPressed)) m_Settings.chunkViewer = !m_Settings.chunkViewer;
    if (KeyPressed(GLFW_KEY_L, m_LWasPressed)) m_Settings.lightVisualizer = !m_Settings.lightVisualizer;

    m_EditTool.HandleBlockSelectKeys(m_Window);

    // Render distance, one chunk per press
    if (KeyPressed(GLFW_KEY_PAGE_UP, m_PageUpWasPressed)) m_Streamer.SetRenderDistance(m_Streamer.RenderDistance() + 1);
    if (KeyPressed(GLFW_KEY_PAGE_DOWN, m_PageDownWasPressed)) m_Streamer.SetRenderDistance(m_Streamer.RenderDistance() - 1);

    m_Player.UpdateMovement(m_Window, deltaTime, m_World);
}

void Application::RunFrame(double frameStartTime, float deltaTime) {
    // Window size and render targets
    glfwGetFramebufferSize(m_Window, &m_WindowWidth, &m_WindowHeight);
    bool minimized = (m_WindowWidth == 0 || m_WindowHeight == 0);
    if (!minimized) m_Renderer.ResizeTargets(m_WindowWidth, m_WindowHeight, m_Settings.renderScale);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // --- World streaming ---
    bool movedChunk = m_Streamer.SetPlayerChunk(m_Player.ChunkCoord());
    m_Streamer.IntegrateResults(frameStartTime, STREAMING_BUDGET_SECONDS);
    if (movedChunk) m_Streamer.UpdateWindows();
    if (movedChunk || m_Editor.ConsumeTerrainChanged()) m_Grass.MarkDirty();
    m_Grass.Update(glfwGetTime(), m_Streamer.PlayerChunk(), m_World);

    // --- Input and gameplay ---
    HandleKeys(deltaTime);
    m_Overlay.UpdateMinimap(m_World, m_Player.Position());
    m_EditTool.HandleMouse(m_Window, m_Player);

    // --- UI ---
    OverlayContext overlay{ m_Window, deltaTime, ImGui::GetIO().Framerate, m_EditTool, m_Streamer, m_Cache, m_World,
        m_Settings, m_Renderer.Targets(), m_Grass.ActiveCount(), GrassAnimator::ANIMATION_RADIUS };
    m_Overlay.Draw(overlay);

    if (minimized) {
        // Nothing to draw into; keep the UI frame balanced and wait for events
        ImGui::Render();
        glfwWaitEvents();
        return;
    }

    if (m_Options.lockCamera) m_Player.LockView(m_Options.cameraVoxel, m_Options.cameraYaw, m_Options.cameraPitch);

    // --- GPU simulation: ocean waves, grass ---
    float animationTime = m_Options.fixedTime >= 0.0f ? m_Options.fixedTime : (float)glfwGetTime();
    m_Ocean.Update(animationTime);
    m_Grass.Animate(animationTime, m_Cache);

    // --- Render ---
    FrameParams frame;
    frame.cameraPos = m_Player.Position();
    frame.cameraFront = m_Player.Front();
    frame.cameraUp = m_Player.Up();
    frame.time = animationTime;
    frame.renderDistance = m_Streamer.RenderDistance();
    frame.windowWidth = m_WindowWidth;
    frame.windowHeight = m_WindowHeight;
    m_Renderer.Render(frame, m_Settings, m_Cache, m_Ocean, m_Shore);

    DrawHud(m_EditTool.CrosshairColor(m_Player), m_EditTool.SelectedBlock());
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
