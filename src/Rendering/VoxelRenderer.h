#pragma once

#include "Rendering/BuildPreview.h"
#include "Rendering/RenderTargets.h"
#include "Rendering/VoxelObject.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

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
    const std::vector<VoxelObject>* objects = nullptr; // Moving voxel objects (boats); up to MAX_OBJECTS
};

// Draws the voxel world with three compute passes (shaders/render/):
//   1. trace.comp  - one camera ray per pixel into a G-buffer
//   2. shadow.comp - soft shadows once per 2x2 pixel quad (optional, see RenderSettings)
//   3. shade.comp  - lighting, water, sky and overlays into the final image
// then stretches the image onto the window. Also refreshes the shore map and binds the ocean
// textures the passes read.
class VoxelRenderer {
public:
    static constexpr int MAX_OBJECTS = 1024;
    static constexpr int OBJECT_TILE = 16;      // Pixels per side of a screen tile for object culling
    static constexpr int OBJECTS_PER_TILE = 31; // ponytail: extra objects in a crowded tile are not drawn there
    // Shadow grid: which objects can shadow a world column (cells of SHADOW_CELL x SHADOW_CELL
    // columns, SHADOW_GRID cells across, around the camera's focus)
    static constexpr int SHADOW_CELL = 16;
    static constexpr int SHADOW_GRID = 128;
    static constexpr int OBJECTS_PER_SHADOW_CELL = 15; // ponytail: extra objects in a crowded cell cast no shadow there
    static constexpr float SHADOW_MAX_PUSH = 96.0f;     // Voxels a shadow may reach from its object (low sun)
    static constexpr float FLOAT_MARGIN = 4.0f;         // Voxels a floating object may sway beyond its box when culled

    bool Init();

    // Adds a voxel object model (call before the first Render); returns its index for VoxelObject::model
    int AddObjectModel(const VoxelObjectModel& model);

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
        GLint previewState, previewMin, previewMax, previewGhost, numObjects, objectTilesX, shadowGridOrigin;
        GLint overlayOrigin, overlayGroundY, overlayTileSize;

        void Locate(GLuint program);
    };

    struct Pass {
        GLuint program = 0;
        PassUniforms uniforms;
    };

    bool LoadPass(Pass& pass, const char* path);

    Pass m_TracePass, m_ShadowPass, m_ShadePass;
    Pass m_FloatPass; // Floats voxel objects on the waves before shading
    GLuint m_GhostTexture = 0;     // The placement preview's model (R8UI, x, z, y)

    // Voxel objects: every model side by side along x in one atlas (R8UI, x, z, y), and the
    // instances of the frame in a storage buffer
    struct ObjectModelSlot {
        int atlasX;
        glm::ivec3 size;
    };
    std::vector<VoxelObjectModel> m_ObjectModels;
    std::vector<ObjectModelSlot> m_ObjectSlots;
    GLuint m_ObjectAtlas = 0;
    GLuint m_ObjectBuffer = 0;
    // Per screen tile: how many objects touch it, then their indices (1 + OBJECTS_PER_TILE each)
    GLuint m_TileBuffer = 0;
    std::vector<uint32_t> m_TileData;
    std::vector<uint8_t> m_GpuObjects; // This frame's GpuObjects, reserved at Init
    GLuint m_ShadowGridBuffer = 0;
    std::vector<uint32_t> m_ShadowGrid; // Per cell: a count, then object indices; sized at Init
    bool m_AtlasDirty = false;
    uint32_t m_GhostRevision = 0;
    RenderTargets m_Targets;
};
