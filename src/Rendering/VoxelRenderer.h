#pragma once

#include "Rendering/BuildPreview.h"
#include "Rendering/RenderTargets.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

class GpuChunkCache;
class OceanSimulation;
class ShoreMap;
class TileOverlay;
struct RenderSettings;

// Everything the renderer needs to know about the current frame
struct FrameParams {
    glm::vec3 cameraPos;   // World units
    glm::vec3 cameraFront;
    glm::vec3 cameraUp;
    glm::vec3 focusPoint;  // Where the world streams around (the shore map follows it)
    float time;            // Drives the day-night cycle and all animation
    int renderDistance;    // Chunks
    int windowWidth, windowHeight;
    BuildPreview preview;  // Highlighted box (building placement or selection)
    const TileOverlay* overlay = nullptr; // Per-tile ground highlights; none when null
};

// Draws the voxel world with three compute passes (shaders/render/):
//   1. trace.comp  - one camera ray per pixel into a G-buffer
//   2. shadow.comp - soft shadows once per 2x2 pixel quad (optional, see RenderSettings)
//   3. shade.comp  - lighting, water, sky and overlays into the final image
// then stretches the image onto the window. Also refreshes the shore map and binds the ocean
// textures the passes read.
class VoxelRenderer {
public:
    bool Init();

    // Matches the render targets to the window size and render scale
    void ResizeTargets(int windowWidth, int windowHeight, float renderScale);

    void Render(const FrameParams& frame, const RenderSettings& settings, const GpuChunkCache& cache,
        const OceanSimulation& ocean, ShoreMap& shore);

    const RenderTargets& Targets() const { return m_Targets; }

private:
    // Uniform locations of one pass (the three passes share one set of uniforms; unused ones are -1)
    struct PassUniforms {
        GLint pageTable, poolBase, chunkViewerEnabled, lightVisualizerEnabled, halfResShadows, renderSize;
        GLint seaLevel, oceanTileSizes, oceanChoppiness, shoreOrigin, renderDistanceVoxels;
        GLint dimX, dimY, dimZ, cameraPos, inverseView, inverseProj, time;
        GLint sunDir, moonDir, lightDir, lightColor, skyColor, ambient;
        GLint previewState, previewMin, previewMax;
        GLint overlayOrigin, overlayGroundY;

        void Locate(GLuint program);
    };

    struct Pass {
        GLuint program = 0;
        PassUniforms uniforms;
    };

    bool LoadPass(Pass& pass, const char* path);

    Pass m_TracePass, m_ShadowPass, m_ShadePass;
    RenderTargets m_Targets;
};
