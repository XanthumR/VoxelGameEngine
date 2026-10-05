#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Gameplay/Picking.h"
#include "Simulation/IslandRegistry.h"

#include <cstdint>
#include <vector>

struct GLFWwindow;
struct RenderSettings;
class ChunkStreamer;
class GpuChunkCache;
class BuildTool;
class DebugEditTool;
class RenderTargets;
class Simulation;
class VoxelWorld;

// Everything the overlay shows or lets the player change
struct OverlayContext {
    GLFWwindow* window;
    float deltaTime;
    float framerate;
    DebugEditTool& editTool;
    ChunkStreamer& streamer;
    const GpuChunkCache& cache;
    const VoxelWorld& world;
    RenderSettings& settings;
    const RenderTargets& targets;
    size_t animatedGrassTufts;
    int grassAnimationRadius;
    const char* cameraMode;
    PickResult hover;
    IslandId hoverIsland;
    Simulation& simulation;
    uint64_t droppedSimulationSteps;
    const BuildTool& buildTool;
};

// The F3 debug window: engine metrics, a top-down minimap and the editor and render controls
class DebugOverlay {
public:
    static constexpr int MINIMAP_SIZE = 64; // Voxels per side

    void Init();
    void UpdateMinimap(const VoxelWorld& world, glm::vec3 playerPosition);
    void Draw(const OverlayContext& context);

    bool& Visible() { return m_Visible; }

private:
    bool m_Visible = true;
    GLuint m_MinimapTexture = 0;
    std::vector<uint8_t> m_MinimapPixels = std::vector<uint8_t>(MINIMAP_SIZE * MINIMAP_SIZE * 4, 0);
};
