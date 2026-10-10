#include "Rendering/FigureRenderer.h"

#include "Rendering/GpuChunkCache.h"
#include "Rendering/ShaderLoader.h"

#include <algorithm>

bool FigureRenderer::Pass::Load(const char* path) {
    program = LoadComputeProgram(path);
    if (!program) return false;
    count = glGetUniformLocation(program, "numFigures");
    pageTable = glGetUniformLocation(program, "pageTable");
    poolBase = glGetUniformLocation(program, "poolBase");
    seaLevel = glGetUniformLocation(program, "seaLevel");
    return true;
}

bool FigureRenderer::Init() {
    if (!m_Erase.Load("people/figures_erase.comp") || !m_Draw.Load("people/figures_draw.comp")) return false;

    // Both buffers hold the most figures there can be, allocated once
    glGenBuffers(2, m_Buffers);
    for (GLuint buffer : m_Buffers) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_FIGURES * sizeof(Figure), nullptr, GL_DYNAMIC_DRAW);
    }
    return true;
}

void FigureRenderer::Dispatch(const Pass& pass, int count, const GpuChunkCache& cache, int seaLevel) {
    if (count == 0) return;
    glUseProgram(pass.program);
    glm::ivec3 poolBase = cache.PoolBaseSlots();
    glUniform3iv(pass.poolBase, 1, &poolBase[0]);
    glUniform1i(pass.seaLevel, seaLevel);
    glUniform1i(pass.pageTable, 1);
    glUniform1i(pass.count, count);
    glDispatchCompute(((GLuint)count + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void FigureRenderer::Draw(const std::vector<Figure>& figures, const GpuChunkCache& cache, int seaLevel) {
    int previous = m_Current;
    int current = 1 - m_Current;
    m_Counts[current] = (int)std::min(figures.size(), (size_t)MAX_FIGURES);
    if (m_Counts[previous] == 0 && m_Counts[current] == 0) return;
    if (m_Counts[current] > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers[current]);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_Counts[current] * sizeof(Figure), figures.data());
    }

    cache.BindAsImages();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, cache.PageTableTexture());
    // Each program reads its own binding, so nothing changes between the two dispatches
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_Buffers[previous]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_Buffers[current]);
    Dispatch(m_Erase, m_Counts[previous], cache, seaLevel); // Erase last frame's figures...
    Dispatch(m_Draw, m_Counts[current], cache, seaLevel);   // ...then draw this frame's
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
    m_Current = current;
}
