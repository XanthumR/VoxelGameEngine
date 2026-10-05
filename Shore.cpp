#include "Shore.h"

#include <cstdlib>
#include <utility>

namespace {

GLuint CreateTexture(GLenum format, GLenum filter) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, format, ShoreMap::SIZE, ShoreMap::SIZE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}

} // namespace

ShoreMap::PassUniforms ShoreMap::Locate(GLuint program) {
    PassUniforms u;
    u.poolBase = glGetUniformLocation(program, "poolBase");
    u.seaLevel = glGetUniformLocation(program, "seaLevel");
    u.shoreOrigin = glGetUniformLocation(program, "shoreOrigin");
    u.jumpStep = glGetUniformLocation(program, "jumpStep");
    return u;
}

void ShoreMap::Init(GLuint seedProgram, GLuint jumpFloodProgram, GLuint finalProgram) {
    m_SeedProgram = seedProgram;
    m_JumpFloodProgram = jumpFloodProgram;
    m_FinalProgram = finalProgram;
    m_SeedUniforms = Locate(seedProgram);
    m_JumpFloodUniforms = Locate(jumpFloodProgram);
    m_FinalUniforms = Locate(finalProgram);

    glActiveTexture(GL_TEXTURE0);
    m_SeedsA = CreateTexture(GL_RGBA32I, GL_NEAREST);
    m_SeedsB = CreateTexture(GL_RGBA32I, GL_NEAREST);
    m_Depth = CreateTexture(GL_R32F, GL_NEAREST);
    m_ShoreTexture = CreateTexture(GL_RGBA16F, GL_LINEAR); // Sampled by the renderer
}

void ShoreMap::SetUniforms(const PassUniforms& u, glm::ivec3 poolBase, int seaLevel, int jumpStep) const {
    glUniform3iv(u.poolBase, 1, &poolBase[0]);
    glUniform1i(u.seaLevel, seaLevel);
    glUniform2i(u.shoreOrigin, m_Origin.x, m_Origin.y);
    glUniform1i(u.jumpStep, jumpStep);
}

void ShoreMap::Update(glm::ivec2 playerColumn, double now, glm::ivec3 poolBase, int seaLevel) {
    glm::ivec2 center = m_Origin + glm::ivec2(SIZE / 2);
    bool moved = std::abs(playerColumn.x - center.x) > RECENTER_DISTANCE || std::abs(playerColumn.y - center.y) > RECENTER_DISTANCE;
    if (m_Built && !moved && now - m_LastBuild < REFRESH_SECONDS) return;

    // Chunk-aligned window centered on the player
    m_Origin = ((playerColumn - glm::ivec2(SIZE / 2)) >> 5) << 5;
    m_Built = true;
    m_LastBuild = now;
    const GLuint groups = SIZE / 8;

    // 1. Seeds: every column is land or water at sea level; water also gets its depth
    glUseProgram(m_SeedProgram);
    SetUniforms(m_SeedUniforms, poolBase, seaLevel, 0);
    glBindImageTexture(5, m_SeedsA, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32I);
    glBindImageTexture(6, m_Depth, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 2. Jump flooding: nearest land and nearest water column for every texel, log2(SIZE) passes
    //    plus a final step of 1 to fix the few errors plain JFA leaves
    glUseProgram(m_JumpFloodProgram);
    GLuint source = m_SeedsA, target = m_SeedsB;
    for (int step = SIZE / 2; step >= 1; step /= 2) {
        SetUniforms(m_JumpFloodUniforms, poolBase, seaLevel, step);
        glBindImageTexture(5, source, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32I);
        glBindImageTexture(7, target, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32I);
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        std::swap(source, target);
    }
    SetUniforms(m_JumpFloodUniforms, poolBase, seaLevel, 1);
    glBindImageTexture(5, source, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32I);
    glBindImageTexture(7, target, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32I);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    std::swap(source, target);

    // 3. Distances, depth and inland direction into the sampled shore texture
    glUseProgram(m_FinalProgram);
    SetUniforms(m_FinalUniforms, poolBase, seaLevel, 0);
    glBindImageTexture(5, source, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32I);
    glBindImageTexture(6, m_Depth, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(7, m_ShoreTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}
