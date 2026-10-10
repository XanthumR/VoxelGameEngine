#pragma once

#include "Gameplay/Figure.h"

#include <glad/glad.h>

#include <vector>

class GpuChunkCache;

// Draws figures (smoke puffs) into the GPU chunk data with
// shaders/people/figures_erase.comp and figures_draw.comp: each frame it erases last frame's
// figures, then draws the current ones. Figures only replace air (or water, for hulls) and only figure voxels are erased, the sea
// being put back below sea level, so the world is never damaged; drawing every frame also restores
// figures wiped by a chunk re-upload.
class FigureRenderer {
public:
    static constexpr int MAX_FIGURES = 1536; // Puffs

    bool Init();
    void Draw(const std::vector<Figure>& figures, const GpuChunkCache& cache, int seaLevel);

private:
    struct Pass {
        GLuint program = 0;
        GLint count = -1, pageTable = -1, poolBase = -1, seaLevel = -1;
        bool Load(const char* path);
    };
    void Dispatch(const Pass& pass, int count, const GpuChunkCache& cache, int seaLevel);

    Pass m_Erase, m_Draw;
    GLuint m_Buffers[2] = { 0, 0 }; // Last frame's figures and this frame's, swapped every frame
    int m_Counts[2] = { 0, 0 };
    int m_Current = 0;
};
