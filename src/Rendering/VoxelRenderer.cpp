#include "Rendering/VoxelRenderer.h"

#include "Rendering/GpuChunkCache.h"
#include "Rendering/OceanSimulation.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/ShaderLoader.h"
#include "Rendering/ShoreMap.h"
#include "Rendering/SkyLighting.h"
#include "Rendering/TileOverlay.h"
#include "World/WorldConstants.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>

namespace {

// One voxel object for the shade pass (std430): where it is and its local axes in the world
struct GpuObject {
    glm::vec4 position; // Middle of the model's bottom face, in voxels
    glm::vec4 axisX, axisY, axisZ;
    glm::ivec4 model;   // Atlas x of the model, its size
};

} // namespace

void VoxelRenderer::PassUniforms::Locate(GLuint program) {
    auto loc = [program](const char* name) { return glGetUniformLocation(program, name); };
    pageTable = loc("pageTable");
    poolBase = loc("poolBase");
    chunkViewerEnabled = loc("chunkViewerEnabled");
    lightVisualizerEnabled = loc("lightVisualizerEnabled");
    halfResShadows = loc("halfResShadows");
    renderSize = loc("renderSize");
    renderDistanceVoxels = loc("renderDistanceVoxels");
    seaLevel = loc("seaLevel");
    oceanTileSizes = loc("oceanTileSizes");
    oceanChoppiness = loc("oceanChoppiness");
    shoreOrigin = loc("shoreOrigin");
    dimX = loc("dimX");
    dimY = loc("dimY");
    dimZ = loc("dimZ");
    cameraPos = loc("cameraPos");
    inverseView = loc("inverseView");
    inverseProj = loc("inverseProj");
    time = loc("time");
    sunDir = loc("sunDir");
    moonDir = loc("moonDir");
    lightDir = loc("lightDir");
    lightColor = loc("lightColor");
    skyColor = loc("skyColor");
    ambient = loc("ambient");
    previewState = loc("previewState");
    previewMin = loc("previewMin");
    previewMax = loc("previewMax");
    previewGhost = loc("previewGhost");
    numObjects = loc("numObjects");
    objectTilesX = loc("objectTilesX");
    overlayOrigin = loc("overlayOrigin");
    overlayGroundY = loc("overlayGroundY");
    overlayTileSize = loc("overlayTileSize");
}

bool VoxelRenderer::LoadPass(Pass& pass, const char* path) {
    pass.program = LoadComputeProgram(path);
    if (!pass.program) return false;
    pass.uniforms.Locate(pass.program);
    return true;
}

bool VoxelRenderer::Init() {
    if (!LoadPass(m_TracePass, "render/trace.comp") ||
        !LoadPass(m_ShadowPass, "render/shadow.comp") ||
        !LoadPass(m_ShadePass, "render/shade.comp")) {
        return false;
    }
    m_Targets.Init(); // Textures are allocated by the first ResizeTargets
    glGenBuffers(1, &m_ObjectBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ObjectBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_OBJECTS * sizeof(GpuObject), nullptr, GL_DYNAMIC_DRAW);
    glGenBuffers(1, &m_TileBuffer);
    m_GpuObjects.reserve(MAX_OBJECTS * sizeof(GpuObject));
    m_ObjectModels.reserve(256);
    m_ObjectSlots.reserve(256);
    return true;
}

int VoxelRenderer::AddObjectModel(const VoxelObjectModel& model) {
    int atlasX = m_ObjectSlots.empty() ? 0 : m_ObjectSlots.back().atlasX + m_ObjectSlots.back().size.x;
    m_ObjectSlots.push_back({ atlasX, model.size });
    m_ObjectModels.push_back(model);
    m_AtlasDirty = true;
    return (int)m_ObjectSlots.size() - 1;
}

void VoxelRenderer::ResizeTargets(int windowWidth, int windowHeight, float renderScale) {
    int targetWidth = std::max(1, (int)std::lround(windowWidth * renderScale));
    int targetHeight = std::max(1, (int)std::lround(windowHeight * renderScale));
    if (targetWidth != m_Targets.Width() || targetHeight != m_Targets.Height()) {
        m_Targets.Resize(targetWidth, targetHeight);
    }
}

void VoxelRenderer::Render(const FrameParams& frame, const RenderSettings& settings, const GpuChunkCache& cache,
    const OceanSimulation& ocean, ShoreMap& shore) {
    glm::mat4 view = glm::lookAt(frame.cameraPos, frame.cameraPos + frame.cameraFront, frame.cameraUp);
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)m_Targets.Width() / (float)m_Targets.Height(), 0.1f, 100.0f);

    // Voxel data (page table on unit 1, pools and brick masks on units 2-9)
    cache.BindForSampling();
    glm::ivec3 poolBase = cache.PoolBaseSlots();

    // Coastline data for the waves (needs the voxel data bound above)
    glm::ivec2 playerColumn = glm::ivec2(glm::floor(glm::vec2(frame.focusPoint.x, frame.focusPoint.z) * VOXELS_PER_UNIT));
    shore.Update(playerColumn, glfwGetTime(), poolBase, SEA_LEVEL);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_2D, shore.Texture());

    // FFT ocean fields on units 10 and 11
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ocean.DisplacementTexture());
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ocean.SlopeTexture());

    // The placement preview's model on unit 14, uploaded when it changes
    glActiveTexture(GL_TEXTURE14);
    bool ghost = frame.preview.ghost != nullptr;
    if (ghost && frame.preview.ghostRevision != m_GhostRevision) {
        if (m_GhostTexture == 0) glGenTextures(1, &m_GhostTexture);
        glm::ivec3 size = frame.preview.max - frame.preview.min;
        glBindTexture(GL_TEXTURE_3D, m_GhostTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, size.x, size.z, size.y, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, frame.preview.ghost->data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        m_GhostRevision = frame.preview.ghostRevision;
    }
    glBindTexture(GL_TEXTURE_3D, m_GhostTexture);

    // Voxel objects: the model atlas on unit 15 (built once the models are in), instances on buffer 6
    if (m_AtlasDirty) {
        glm::ivec3 atlasSize(0);
        for (const ObjectModelSlot& slot : m_ObjectSlots) atlasSize = glm::ivec3(slot.atlasX + slot.size.x, std::max(atlasSize.y, slot.size.y), std::max(atlasSize.z, slot.size.z));
        std::vector<uint8_t> atlas((size_t)atlasSize.x * atlasSize.y * atlasSize.z, 0);
        for (size_t m = 0; m < m_ObjectModels.size(); m++) {
            const glm::ivec3 size = m_ObjectSlots[m].size;
            for (int y = 0; y < size.y; y++) {
                for (int z = 0; z < size.z; z++) {
                    for (int x = 0; x < size.x; x++) {
                        atlas[(size_t)(m_ObjectSlots[m].atlasX + x) + (size_t)atlasSize.x * ((size_t)z + (size_t)atlasSize.z * (size_t)y)] =
                            m_ObjectModels[m].ids[(size_t)x + (size_t)size.x * ((size_t)z + (size_t)size.z * (size_t)y)];
                    }
                }
            }
        }
        if (m_ObjectAtlas == 0) glGenTextures(1, &m_ObjectAtlas);
        glBindTexture(GL_TEXTURE_3D, m_ObjectAtlas);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, atlasSize.x, atlasSize.z, atlasSize.y, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, atlas.data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        m_AtlasDirty = false;
    }
    glActiveTexture(GL_TEXTURE15);
    glBindTexture(GL_TEXTURE_3D, m_ObjectAtlas);
    // Each object goes into the lists of the screen tiles its box covers, so a pixel only tests
    // the few objects near it
    int tilesX = (m_Targets.Width() + OBJECT_TILE - 1) / OBJECT_TILE, tilesY = (m_Targets.Height() + OBJECT_TILE - 1) / OBJECT_TILE;
    const size_t tileStride = 1 + OBJECTS_PER_TILE;
    if (m_TileData.size() != (size_t)tilesX * tilesY * tileStride) {
        m_TileData.assign((size_t)tilesX * tilesY * tileStride, 0); // Only when the render size changes
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TileBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_TileData.size() * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    }
    for (size_t tile = 0; tile < (size_t)tilesX * tilesY; tile++) m_TileData[tile * tileStride] = 0;

    int objectCount = 0;
    m_GpuObjects.clear();
    if (frame.objects && m_ObjectAtlas != 0) {
        glm::mat4 viewProjection = projection * view;
        for (const VoxelObject& object : *frame.objects) {
            if (objectCount == MAX_OBJECTS || object.model < 0 || object.model >= (int)m_ObjectSlots.size()) continue;
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), object.yaw, glm::vec3(0, 1, 0));
            rotation = glm::rotate(rotation, -object.pitch, glm::vec3(1, 0, 0));
            rotation = glm::rotate(rotation, object.roll, glm::vec3(0, 0, 1));
            const ObjectModelSlot& slot = m_ObjectSlots[object.model];

            // The box's screen rectangle (all of the screen when a corner is behind the camera)
            glm::vec2 low(1e9f), high(-1e9f);
            bool behind = false, inFront = false;
            for (int corner = 0; corner < 8; corner++) {
                glm::vec3 local((corner & 1) ? 0.5f : -0.5f, (corner & 2) ? 1.0f : 0.0f, (corner & 4) ? 0.5f : -0.5f);
                local *= glm::vec3(slot.size);
                glm::vec3 world = object.position + glm::vec3(rotation[0]) * local.x + glm::vec3(rotation[1]) * local.y + glm::vec3(rotation[2]) * local.z;
                glm::vec4 clip = viewProjection * glm::vec4(world / VOXELS_PER_UNIT, 1.0f);
                if (clip.w <= 0.001f) {
                    behind = true;
                    continue;
                }
                inFront = true;
                glm::vec2 pixel = (glm::vec2(clip) / clip.w * 0.5f + 0.5f) * glm::vec2(m_Targets.Width(), m_Targets.Height());
                low = glm::min(low, pixel);
                high = glm::max(high, pixel);
            }
            if (!inFront) continue;
            glm::ivec2 tileLow = behind ? glm::ivec2(0) : glm::ivec2(glm::floor(low)) / OBJECT_TILE;
            glm::ivec2 tileHigh = behind ? glm::ivec2(tilesX - 1, tilesY - 1) : glm::ivec2(glm::floor(high)) / OBJECT_TILE;
            tileLow = glm::max(tileLow, glm::ivec2(0));
            tileHigh = glm::min(tileHigh, glm::ivec2(tilesX - 1, tilesY - 1));
            if (tileLow.x > tileHigh.x || tileLow.y > tileHigh.y) continue; // Off screen

            GpuObject gpu = { glm::vec4(object.position, 0.0f), rotation[0], rotation[1], rotation[2], glm::ivec4(slot.atlasX, slot.size) };
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&gpu);
            m_GpuObjects.insert(m_GpuObjects.end(), bytes, bytes + sizeof(GpuObject)); // Within the reserve
            for (int ty = tileLow.y; ty <= tileHigh.y; ty++) {
                for (int tx = tileLow.x; tx <= tileHigh.x; tx++) {
                    uint32_t* list = &m_TileData[((size_t)ty * tilesX + tx) * tileStride];
                    if (list[0] < (uint32_t)OBJECTS_PER_TILE) list[1 + list[0]++] = (uint32_t)objectCount;
                }
            }
            objectCount++;
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ObjectBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_GpuObjects.size(), m_GpuObjects.data());
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TileBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_TileData.size() * sizeof(uint32_t), m_TileData.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_ObjectBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_TileBuffer);

    // Tile highlights on unit 13
    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, frame.overlay ? frame.overlay->Texture() : 0);
    glm::ivec2 overlayOrigin = frame.overlay ? frame.overlay->Origin() : glm::ivec2(0);

    m_Targets.BindImages();

    SkyLighting sky = SkyLighting::At(frame.time);
    glm::mat4 inverseView = glm::inverse(view);
    glm::mat4 inverseProjection = glm::inverse(projection);

    // Uniforms are per program, so each pass gets the same set
    auto uploadUniforms = [&](const PassUniforms& u) {
        glUniform1i(u.pageTable, 1); // Unit 1
        glUniform3iv(u.poolBase, 1, &poolBase[0]); // First slot of pools 1..3
        glUniform2i(u.renderSize, m_Targets.Width(), m_Targets.Height());
        glUniform1i(u.halfResShadows, settings.halfResShadows ? 1 : 0);
        glUniform1i(u.chunkViewerEnabled, settings.chunkViewer ? 1 : 0);
        glUniform1i(u.lightVisualizerEnabled, settings.lightVisualizer ? 1 : 0);

        // Rays stop one chunk past the upload radius (18 chunks -> 608 voxels)
        glUniform1i(u.renderDistanceVoxels, (frame.renderDistance + 1) * CHUNK_SIZE);
        glUniform1i(u.seaLevel, SEA_LEVEL);
        glm::vec3 tileSizes = ocean.TileSizes();
        glUniform3fv(u.oceanTileSizes, 1, &tileSizes[0]);
        glUniform1f(u.oceanChoppiness, ocean.Choppiness());
        glm::ivec2 shoreOrigin = shore.Origin();
        glUniform2i(u.shoreOrigin, shoreOrigin.x, shoreOrigin.y);

        // dimX is the world->voxel scale in the shader; dimY the vertical extent
        glUniform1i(u.dimX, (int)VOXELS_PER_UNIT);
        glUniform1i(u.dimY, WORLD_HEIGHT);
        glUniform1i(u.dimZ, (int)VOXELS_PER_UNIT);
        glUniform3fv(u.cameraPos, 1, &frame.cameraPos[0]);
        glUniformMatrix4fv(u.inverseView, 1, GL_FALSE, glm::value_ptr(inverseView));
        glUniformMatrix4fv(u.inverseProj, 1, GL_FALSE, glm::value_ptr(inverseProjection));
        glUniform1f(u.time, frame.time);

        // Day-night cycle
        glUniform3fv(u.sunDir, 1, &sky.sunDir[0]);
        glUniform3fv(u.moonDir, 1, &sky.moonDir[0]);
        glUniform3fv(u.lightDir, 1, &sky.lightDir[0]);
        glUniform3fv(u.lightColor, 1, &sky.lightColor[0]);
        glUniform3fv(u.skyColor, 1, &sky.skyColor[0]);
        glUniform1f(u.ambient, sky.ambient);

        // Build preview (shade pass only; the other passes have no such uniforms, location -1)
        glUniform1i(u.previewState, frame.preview.state);
        glUniform3iv(u.previewMin, 1, &frame.preview.min[0]);
        glUniform3iv(u.previewMax, 1, &frame.preview.max[0]);
        glUniform1i(u.previewGhost, ghost && m_GhostTexture != 0 ? 1 : 0);
        glUniform1i(u.numObjects, objectCount);
        glUniform1i(u.objectTilesX, tilesX);
        glUniform2i(u.overlayOrigin, overlayOrigin.x, overlayOrigin.y);
        // Buildings and roads stand on SEA_LEVEL + ISLAND_HEIGHT; no overlay draws below the world
        glUniform1i(u.overlayGroundY, frame.overlay ? SEA_LEVEL + ISLAND_HEIGHT : -1000);
        glUniform1i(u.overlayTileSize, frame.overlay ? frame.overlay->TileSize() : 1);
    };

    GLuint fullGroupsX = (m_Targets.Width() + 7) / 8, fullGroupsY = (m_Targets.Height() + 7) / 8;

    // 1. Trace: one camera ray per pixel into the G-buffer
    glUseProgram(m_TracePass.program);
    uploadUniforms(m_TracePass.uniforms);
    glDispatchCompute(fullGroupsX, fullGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 2. Shadows: soft shadows once per 2x2 quad
    if (settings.halfResShadows) {
        glUseProgram(m_ShadowPass.program);
        uploadUniforms(m_ShadowPass.uniforms);
        int quadsX = (m_Targets.Width() + 1) / 2, quadsY = (m_Targets.Height() + 1) / 2;
        glDispatchCompute((quadsX + 7) / 8, (quadsY + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    // 3. Shade: lighting and overlays into the final color image
    glUseProgram(m_ShadePass.program);
    uploadUniforms(m_ShadePass.uniforms);
    glDispatchCompute(fullGroupsX, fullGroupsY, 1);

    // Make the image writes visible to the blit
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    m_Targets.BlitToWindow(frame.windowWidth, frame.windowHeight);
}
