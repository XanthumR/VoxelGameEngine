#include "Rendering/GrassAnimator.h"

#include "Rendering/GpuChunkCache.h"
#include "Rendering/ShaderLoader.h"
#include "World/BlockTypes.h"
#include "World/GrassTufts.h"
#include "World/TerrainGenerator.h"
#include "World/VoxelWorld.h"
#include "World/WorldConstants.h"

#include <unordered_map>

namespace {

const glm::vec2 WIND_DIR = glm::normalize(glm::vec2(1.0f, 0.35f));
const int REST_POSE = 8 | (8 << 4); // encodePose(0, 0) in grass_animate.comp

uint64_t ColumnKey(int wx, int wz) {
    return ((uint64_t)(uint32_t)wx << 32) | (uint32_t)wz;
}

} // namespace

bool GrassAnimator::Init(TerrainGenerator& placement) {
    m_Placement = &placement;
    m_Program = LoadComputeProgram("grass/grass_animate.comp");
    if (!m_Program) return false;
    m_TimeLocation = glGetUniformLocation(m_Program, "time");
    m_NumPlantsLocation = glGetUniformLocation(m_Program, "numPlants");
    m_PageTableLocation = glGetUniformLocation(m_Program, "pageTable");
    m_PoolBaseLocation = glGetUniformLocation(m_Program, "poolBase");
    m_ModeLocation = glGetUniformLocation(m_Program, "mode");
    m_WindDirLocation = glGetUniformLocation(m_Program, "windDir");
    glGenBuffers(1, &m_PlantBuffer); // Filled by Rebuild
    return true;
}

void GrassAnimator::Update(double now, glm::ivec3 playerChunk, const VoxelWorld& world) {
    if (GRASS_TUFTS_ENABLED && m_Dirty && now - m_LastRebuild >= REBUILD_INTERVAL) Rebuild(now, playerChunk, world);
}

// Reads the current poses back from the GPU first, so tufts that stay in range keep animating
// from where they are
void GrassAnimator::Rebuild(double now, glm::ivec3 playerChunk, const VoxelWorld& world) {
    std::unordered_map<uint64_t, AnimatedPlant> previous;
    if (!m_Plants.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PlantBuffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_Plants.size() * sizeof(AnimatedPlant), m_Plants.data());
        for (const AnimatedPlant& plant : m_Plants) {
            if (plant.params.z != 0.0f) continue; // Retired last time; already back at rest
            previous[ColumnKey(plant.posAndPose.x, plant.posAndPose.z)] = plant;
        }
    }

    std::vector<AnimatedPlant> next;
    int x0 = (playerChunk.x - ANIMATION_RADIUS) * CHUNK_SIZE, x1 = (playerChunk.x + ANIMATION_RADIUS + 1) * CHUNK_SIZE - 1;
    int z0 = (playerChunk.z - ANIMATION_RADIUS) * CHUNK_SIZE, z1 = (playerChunk.z + ANIMATION_RADIUS + 1) * CHUNK_SIZE - 1;
    for (int gz = z0 >> 2; gz <= z1 >> 2; gz++) {
        for (int gx = x0 >> 2; gx <= x1 >> 2; gx++) {
            GrassTuft tuft;
            if (!m_Placement->GrassTuftInCell(gx, gz, tuft)) continue;

            // Skip tufts whose grass block was dug out (only checkable where CPU data is loaded)
            glm::ivec3 below = tuft.root - glm::ivec3(0, 1, 0);
            if (world.FindChunk(below.x >> 5, below.y >> 5, below.z >> 5) && world.GetVoxel(below.x, below.y, below.z) != Block::GRASS) continue;

            AnimatedPlant plant;
            plant.posAndPose = glm::ivec4(tuft.root, REST_POSE); // Rest pose is already in the chunk data
            plant.params = glm::vec4(tuft.phase, (float)tuft.variant, 0.0f, 0.0f);
            auto it = previous.find(ColumnKey(tuft.root.x, tuft.root.z));
            if (it != previous.end()) {
                plant.posAndPose.w = it->second.posAndPose.w;
                previous.erase(it);
            }
            next.push_back(plant);
        }
    }

    // Tufts that left the radius go back to the rest pose on the GPU
    for (auto& entry : previous) {
        AnimatedPlant plant = entry.second;
        if (plant.posAndPose.w == REST_POSE) continue;
        plant.params.z = 1.0f; // Retire
        next.push_back(plant);
    }

    m_Plants = std::move(next);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PlantBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, m_Plants.size() * sizeof(AnimatedPlant), m_Plants.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_PlantBuffer);
    m_Dirty = false;
    m_LastRebuild = now;
}

void GrassAnimator::Animate(float time, const GpuChunkCache& cache) {
    if (m_Plants.empty()) return;

    glUseProgram(m_Program);
    glUniform1f(m_TimeLocation, time);
    glUniform1i(m_NumPlantsLocation, (int)m_Plants.size());
    glUniform2f(m_WindDirLocation, WIND_DIR.x, WIND_DIR.y);
    glm::ivec3 poolBase = cache.PoolBaseSlots();
    glUniform3iv(m_PoolBaseLocation, 1, &poolBase[0]);

    cache.BindAsImages();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, cache.PageTableTexture());
    glUniform1i(m_PageTableLocation, 1);

    GLuint numGroups = ((GLuint)m_Plants.size() + 63) / 64;

    // Erase the old pose of every tuft that moved...
    glUniform1i(m_ModeLocation, 0);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    // ...then draw every tuft, so overlapping neighbours are always restored
    glUniform1i(m_ModeLocation, 1);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
}
