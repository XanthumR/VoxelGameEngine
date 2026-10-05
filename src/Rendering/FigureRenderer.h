#pragma once

#include "Gameplay/Figure.h"

#include <glad/glad.h>

#include <vector>

class GpuChunkCache;

// Draws figures (walkers, smoke puffs, boats) into the GPU chunk data with
// shaders/people/figures.comp: each frame it erases last frame's figures, then draws the current
// ones. Figures only replace air (or water, for hulls) and only figure voxels are erased, the sea
// being put back below sea level, so the world is never damaged; drawing every frame also restores
// figures wiped by a chunk re-upload.
class FigureRenderer {
public:
    static constexpr int MAX_FIGURES = 512 + 1536 + 64; // Walkers, puffs, boats

    bool Init();
    void Draw(const std::vector<Figure>& figures, const GpuChunkCache& cache, int seaLevel);

private:
    void Dispatch(GLuint buffer, int count, int mode);

    GLuint m_Program = 0;
    GLuint m_Buffers[2] = { 0, 0 }; // Last frame's figures and this frame's, swapped every frame
    int m_Counts[2] = { 0, 0 };
    int m_Current = 0;
    GLint m_ModeLocation = -1, m_CountLocation = -1, m_PageTableLocation = -1, m_PoolBaseLocation = -1, m_SeaLevelLocation = -1;
};
