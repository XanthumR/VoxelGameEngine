#include "Rendering/WalkerRenderer.h"

#include "Rendering/GpuChunkCache.h"
#include "Rendering/ShaderLoader.h"

#include <algorithm>

bool WalkerRenderer::Init() {
    m_Program = LoadComputeProgram("people/walkers.comp");
    if (!m_Program) return false;
    m_ModeLocation = glGetUniformLocation(m_Program, "mode");
    m_CountLocation = glGetUniformLocation(m_Program, "numFigures");
    m_PageTableLocation = glGetUniformLocation(m_Program, "pageTable");
    m_PoolBaseLocation = glGetUniformLocation(m_Program, "poolBase");

    // Both buffers hold the most walkers there can be, allocated once
    glGenBuffers(2, m_Buffers);
    for (GLuint buffer : m_Buffers) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, WalkerSystem::MAX_WALKERS * sizeof(WalkerFigure), nullptr, GL_DYNAMIC_DRAW);
    }
    return true;
}

void WalkerRenderer::Dispatch(GLuint buffer, int count, int mode) {
    if (count == 0) return;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, buffer);
    glUniform1i(m_ModeLocation, mode);
    glUniform1i(m_CountLocation, count);
    glDispatchCompute(((GLuint)count + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void WalkerRenderer::Draw(const std::vector<WalkerFigure>& figures, const GpuChunkCache& cache) {
    int previous = m_Current;
    int current = 1 - m_Current;
    m_Counts[current] = (int)std::min(figures.size(), (size_t)WalkerSystem::MAX_WALKERS);
    if (m_Counts[previous] == 0 && m_Counts[current] == 0) return;

    if (m_Counts[current] > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers[current]);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_Counts[current] * sizeof(WalkerFigure), figures.data());
    }

    glUseProgram(m_Program);
    glm::ivec3 poolBase = cache.PoolBaseSlots();
    glUniform3iv(m_PoolBaseLocation, 1, &poolBase[0]);
    cache.BindAsImages();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, cache.PageTableTexture());
    glUniform1i(m_PageTableLocation, 1);

    Dispatch(m_Buffers[previous], m_Counts[previous], 0); // Erase last frame's figures...
    Dispatch(m_Buffers[current], m_Counts[current], 1);   // ...then draw this frame's
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
    m_Current = current;
}
