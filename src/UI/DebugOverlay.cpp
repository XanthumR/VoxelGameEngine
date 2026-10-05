#include "UI/DebugOverlay.h"

#include "Gameplay/DebugEditTool.h"
#include "Rendering/GpuChunkCache.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/RenderTargets.h"
#include "World/BlockTypes.h"
#include "World/ChunkStreamer.h"
#include "World/VoxelWorld.h"

#include <GLFW/glfw3.h>
#include "imgui.h"

#include <cmath>
#include <cstdint>

void DebugOverlay::Init() {
    glGenTextures(1, &m_MinimapTexture);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_MinimapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MINIMAP_SIZE, MINIMAP_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// Top-down slice 2 voxels below the player, centered on the player
void DebugOverlay::UpdateMinimap(const VoxelWorld& world, glm::vec3 playerPosition) {
    glm::ivec3 p = glm::ivec3(glm::floor(playerPosition * VOXELS_PER_UNIT));
    int startX = p.x - MINIMAP_SIZE / 2;
    int startZ = p.z - MINIMAP_SIZE / 2;
    int checkY = p.y - 2;

    for (int mz = 0; mz < MINIMAP_SIZE; mz++) {
        for (int mx = 0; mx < MINIMAP_SIZE; mx++) {
            uint8_t block = world.GetVoxel(startX + mx, checkY, startZ + mz);

            uint8_t r = 15, g = 15, b = 20, a = 255; // Background
            if (block == Block::GRASS) { r = 34; g = 139; b = 34; }
            else if (block == Block::DIRT) { r = 139; g = 69; b = 19; }
            else if (block == Block::STONE) { r = 128; g = 128; b = 128; }
            else if (block == Block::SAND) { r = 218; g = 165; b = 32; }
            else if (block == Block::CAVERN_GLOW) { r = 0; g = 255; b = 200; }
            else if (block == Block::WATER) { r = 30; g = 110; b = 200; }

            if (mx >= 30 && mx <= 33 && mz >= 30 && mz <= 33) {
                r = 255; g = 255; b = 255; // Player dot
            }

            int idx = (mz * MINIMAP_SIZE + mx) * 4;
            m_MinimapPixels[idx] = r;
            m_MinimapPixels[idx + 1] = g;
            m_MinimapPixels[idx + 2] = b;
            m_MinimapPixels[idx + 3] = a;
        }
    }

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_MinimapTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MINIMAP_SIZE, MINIMAP_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, m_MinimapPixels.data());
}

void DebugOverlay::Draw(const OverlayContext& c) {
    if (!m_Visible) return;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 560), ImGuiCond_FirstUseEver);
    ImGui::Begin("Voxel Anno: Debug", &m_Visible);

    const double minimapBytes = MINIMAP_SIZE * MINIMAP_SIZE * 4.0;
    double vramMB = (c.cache.PoolBytes() + c.cache.PageTableBytes() + minimapBytes + c.targets.MemoryBytes()) / (1024.0 * 1024.0);
    glm::ivec3 playerChunk = c.streamer.PlayerChunk();
    ImGui::Text("--- ENGINE METRICS ---");
    ImGui::Text("Frame time: %.2f ms (%.1f FPS)", c.deltaTime * 1000.0f, c.framerate);
    ImGui::Text("Dispatches: %d | VRAM: %.1f MB", c.settings.halfResShadows ? 3 : 2, vramMB);
    ImGui::Text("Render Resolution: %d x %d", c.targets.Width(), c.targets.Height());
    ImGui::Text("Pending Chunk Gen (Hot): %zu", c.streamer.PendingRequests());
    ImGui::Text("GPU Chunks: %zu / %d slots (%d pool%s)%s", c.cache.ResidentCount(), c.cache.TotalSlots(), c.cache.PoolCount(),
        c.cache.PoolCount() == 1 ? "" : "s", c.cache.IsExhausted() ? " [VRAM FULL]" : "");
    ImGui::Text("CPU Chunks: %zu", c.world.ChunkCount());
    ImGui::Text("Animated Grass Tufts: %zu (within %d chunks)", c.animatedGrassTufts, c.grassAnimationRadius);
    ImGui::Text("Focus Chunk: (%d, %d, %d)", playerChunk.x, playerChunk.y, playerChunk.z);
    ImGui::Text("Camera: %s", c.cameraMode);
    if (c.hover.hit) {
        ImGui::Text("Hovered: voxel (%d, %d, %d), block %d%s", c.hover.voxel.x, c.hover.voxel.y, c.hover.voxel.z,
            c.hover.block, c.hover.block == Block::WATER ? " (water)" : "");
    } else {
        ImGui::Text("Hovered: nothing");
    }
    ImGui::Text("Simulation: tick %llu (%.1f s at 10 Hz), %llu dropped", (unsigned long long)c.simulationTick,
        c.simulationSeconds, (unsigned long long)c.droppedSimulationSteps);

    ImGui::Separator();

    ImGui::Text("--- MINIMAP ---");
    ImGui::Image((ImTextureID)(intptr_t)m_MinimapTexture, ImVec2(128, 128));

    ImGui::Separator();

    ImGui::Text("--- CONTROLS & EDITOR ---");
    ImGui::Text("Strategy: WASD/edge pan, Q/E rotate, wheel zoom, middle-drag pan");
    if (glfwGetInputMode(c.window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Free-fly: [TAB] frees the mouse for the UI.");
    }
    ImGui::Text("Free-fly: left click digs, right click places.");
    ImGui::Checkbox("Chunk Viewer [C]", &c.settings.chunkViewer);
    ImGui::Checkbox("Light Visualizer [L]", &c.settings.lightVisualizer);

    int renderDistance = c.streamer.RenderDistance();
    if (ImGui::SliderInt("Render Distance [PgUp/PgDn]", &renderDistance, ChunkStreamer::MIN_RENDER_DISTANCE, ChunkStreamer::MAX_RENDER_DISTANCE, "%d chunks")) {
        c.streamer.SetRenderDistance(renderDistance);
    }

    int renderScalePercent = (int)std::lround(c.settings.renderScale * 100.0f);
    if (ImGui::SliderInt("Render Scale", &renderScalePercent, (int)(RenderSettings::MIN_RENDER_SCALE * 100.0f), 100, "%d%%")) {
        c.settings.renderScale = renderScalePercent / 100.0f; // Targets are reallocated next frame
    }
    ImGui::Checkbox("Half-Res Shadows", &c.settings.halfResShadows);

    ImGui::Separator();

    ImGui::Text("Selected Voxel Material:");
    const char* materials[] = { "Grass", "Dirt", "Stone", "Sand", "Plant" };
    int& selected = c.editTool.SelectedBlock();
    for (int i = 0; i < 5; i++) {
        if (ImGui::RadioButton(materials[i], selected == (i + 1))) selected = i + 1;
        if (i < 4) ImGui::SameLine();
    }

    ImGui::End();
}
