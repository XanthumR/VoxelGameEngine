#include "Rendering/VoxelRenderer.h"

#include "Rendering/GpuChunkCache.h"
#include "Rendering/OceanSimulation.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/ShaderLoader.h"
#include "Rendering/ShoreMap.h"
#include "Rendering/SkyLighting.h"
#include "World/WorldConstants.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <string>

void VoxelRenderer::PassUniforms::Locate(GLuint program) {
    auto loc = [program](const char* name) { return glGetUniformLocation(program, name); };
    pageTable = loc("pageTable");
    poolBase = loc("poolBase");
    chunkViewerEnabled = loc("chunkViewerEnabled");
    lightVisualizerEnabled = loc("lightVisualizerEnabled");
    laserBeamActive = loc("laserBeamActive");
    laserBeamStart = loc("laserBeamStart");
    laserBeamEnd = loc("laserBeamEnd");
    laserBeamColor = loc("laserBeamColor");
    numFlares = loc("numFlares");
    for (int i = 0; i < SceneVisuals::MAX_FLARES; i++) {
        std::string base = "flares[" + std::to_string(i) + "].";
        flarePos[i] = loc((base + "pos").c_str());
        flareColor[i] = loc((base + "color").c_str());
        flareIntensity[i] = loc((base + "intensity").c_str());
    }
    halfResShadows = loc("halfResShadows");
    renderSize = loc("renderSize");
    heldClumpActive = loc("heldClumpActive");
    heldClumpPos = loc("heldClumpPos");
    heldClumpRadius = loc("heldClumpRadius");
    heldClumpIsArtifact = loc("heldClumpIsArtifact");
    renderDistanceVoxels = loc("renderDistanceVoxels");
    seaLevel = loc("seaLevel");
    oceanTileSizes = loc("oceanTileSizes");
    oceanChoppiness = loc("oceanChoppiness");
    shoreOrigin = loc("shoreOrigin");
    beaconPos = loc("beaconPos");
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
    return true;
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
    glm::ivec2 playerColumn = glm::ivec2(glm::floor(glm::vec2(frame.cameraPos.x, frame.cameraPos.z) * VOXELS_PER_UNIT));
    shore.Update(playerColumn, glfwGetTime(), poolBase, SEA_LEVEL);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_2D, shore.Texture());

    // FFT ocean fields on units 10 and 11
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ocean.DisplacementTexture());
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ocean.SlopeTexture());

    m_Targets.BindImages();

    SkyLighting sky = SkyLighting::At(frame.time);
    glm::mat4 inverseView = glm::inverse(view);
    glm::mat4 inverseProjection = glm::inverse(projection);
    const SceneVisuals& visuals = frame.visuals;

    // Uniforms are per program, so each pass gets the same set
    auto uploadUniforms = [&](const PassUniforms& u) {
        glUniform1i(u.pageTable, 1); // Unit 1
        glUniform3iv(u.poolBase, 1, &poolBase[0]); // First slot of pools 1..3
        glUniform2i(u.renderSize, m_Targets.Width(), m_Targets.Height());
        glUniform1i(u.halfResShadows, settings.halfResShadows ? 1 : 0);
        glUniform1i(u.chunkViewerEnabled, settings.chunkViewer ? 1 : 0);
        glUniform1i(u.lightVisualizerEnabled, settings.lightVisualizer ? 1 : 0);

        // Player tools: beam, flares, held clump
        glUniform1i(u.laserBeamActive, visuals.beam.active ? 1 : 0);
        glUniform3fv(u.laserBeamStart, 1, &visuals.beam.start[0]);
        glUniform3fv(u.laserBeamEnd, 1, &visuals.beam.end[0]);
        glUniform3fv(u.laserBeamColor, 1, &visuals.beam.color[0]);

        glUniform1i(u.numFlares, visuals.flareCount);
        for (int i = 0; i < visuals.flareCount; i++) {
            glUniform3fv(u.flarePos[i], 1, &visuals.flares[i].position[0]);
            glUniform3fv(u.flareColor[i], 1, &visuals.flares[i].color[0]);
            glUniform1f(u.flareIntensity[i], visuals.flares[i].intensity);
        }

        glUniform1i(u.heldClumpActive, visuals.clump.active ? 1 : 0);
        glUniform3fv(u.heldClumpPos, 1, &visuals.clump.position[0]);
        glUniform1f(u.heldClumpRadius, visuals.clump.radius);
        glUniform1i(u.heldClumpIsArtifact, visuals.clump.isArtifact ? 1 : 0);

        // Rays stop one chunk past the upload radius (18 chunks -> 608 voxels)
        glUniform1i(u.renderDistanceVoxels, (frame.renderDistance + 1) * CHUNK_SIZE);
        glUniform1i(u.seaLevel, SEA_LEVEL);
        glm::vec3 tileSizes = ocean.TileSizes();
        glUniform3fv(u.oceanTileSizes, 1, &tileSizes[0]);
        glUniform1f(u.oceanChoppiness, ocean.Choppiness());
        glm::ivec2 shoreOrigin = shore.Origin();
        glUniform2i(u.shoreOrigin, shoreOrigin.x, shoreOrigin.y);
        glm::vec3 beacon = glm::vec3(frame.beaconVoxel.x, 0, frame.beaconVoxel.z) / VOXELS_PER_UNIT;
        glUniform3fv(u.beaconPos, 1, &beacon[0]);

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
