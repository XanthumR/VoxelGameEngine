#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

// Coastline data for the waves, around the player: per voxel column the signed distance to the
// coast (+ over water, - on land), the water depth, and the direction inland. Built on the GPU
// from the voxel data (default.comp PASS_SHORE_*): seed pass, jump flooding, final pass.
class ShoreMap {
public:
    static constexpr int SIZE = 1024;           // Columns per side (1 texel = 1 voxel column)
    static constexpr int RECENTER_DISTANCE = 128; // Rebuild when the player is this far from the center
    static constexpr double REFRESH_SECONDS = 3.0; // Also rebuild this often (streamed chunks, edits)

    // Takes the PASS_SHORE_SEED, PASS_SHORE_JFA and PASS_SHORE_FINAL programs. Allocates up front.
    void Init(GLuint seedProgram, GLuint jumpFloodProgram, GLuint finalProgram);

    // Rebuilds if needed. The page table and chunk pool textures must already be bound.
    void Update(glm::ivec2 playerColumn, double now, glm::ivec3 poolBase, int seaLevel);

    GLuint Texture() const { return m_ShoreTexture; } // RGBA16F: signed distance, depth, inland x, inland z
    glm::ivec2 Origin() const { return m_Origin; }    // World column of texel (0, 0)

private:
    struct PassUniforms {
        GLint poolBase = -1, seaLevel = -1, shoreOrigin = -1, jumpStep = -1;
    };
    static PassUniforms Locate(GLuint program);
    void SetUniforms(const PassUniforms& u, glm::ivec3 poolBase, int seaLevel, int jumpStep) const;

    GLuint m_SeedProgram = 0, m_JumpFloodProgram = 0, m_FinalProgram = 0;
    PassUniforms m_SeedUniforms, m_JumpFloodUniforms, m_FinalUniforms;

    GLuint m_SeedsA = 0, m_SeedsB = 0; // RGBA32I: nearest land column xy, nearest water column zw
    GLuint m_Depth = 0;                // R32F: water depth below sea level
    GLuint m_ShoreTexture = 0;

    glm::ivec2 m_Origin = glm::ivec2(0);
    bool m_Built = false;
    double m_LastBuild = 0.0;
};
