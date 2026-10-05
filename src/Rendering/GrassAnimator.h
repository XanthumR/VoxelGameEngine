#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

class GpuChunkCache;
class TerrainGenerator;
class VoxelWorld;

// One animated grass tuft, 32 bytes to match the std430 layout in grass_animate.comp
struct AnimatedPlant {
    glm::ivec4 posAndPose; // xyz = root (first air voxel above the ground), w = current pose code
    glm::vec4 params;      // x = wind phase offset, y = blade variant (0..3), z = 1 if retired (return to rest), w = unused
};

// Wind animation for the grass tufts near the player. Every tuft is baked into the chunk data in
// its rest pose by the terrain generator; the ones within ANIMATION_RADIUS chunks are handed to
// shaders/grass/grass_animate.comp, which bends them on the GPU. Tufts that leave the radius are
// "retired": the GPU puts them back in the rest pose and the next rebuild drops them.
// Does nothing while GRASS_TUFTS_ENABLED is false.
class GrassAnimator {
public:
    static constexpr int ANIMATION_RADIUS = 4;          // Chunks
    static constexpr double REBUILD_INTERVAL = 0.25;    // Seconds between rebuilds caused by edits

    // placement: a main-thread terrain generator, used to find the tufts
    bool Init(TerrainGenerator& placement);

    void MarkDirty() { m_Dirty = true; } // Player moved to another chunk or the terrain changed

    // Rebuilds the list of animated tufts when needed
    void Update(double now, glm::ivec3 playerChunk, const VoxelWorld& world);

    // Runs the animation on the GPU (erase moved tufts, then draw all)
    void Animate(float time, const GpuChunkCache& cache);

    size_t ActiveCount() const { return m_Plants.size(); }

private:
    void Rebuild(double now, glm::ivec3 playerChunk, const VoxelWorld& world);

    TerrainGenerator* m_Placement = nullptr;
    std::vector<AnimatedPlant> m_Plants;
    GLuint m_Program = 0;
    GLuint m_PlantBuffer = 0; // SSBO, binding 3
    GLint m_TimeLocation = -1, m_NumPlantsLocation = -1, m_PageTableLocation = -1;
    GLint m_PoolBaseLocation = -1, m_ModeLocation = -1, m_WindDirLocation = -1;
    bool m_Dirty = true;
    double m_LastRebuild = -1.0;
};
