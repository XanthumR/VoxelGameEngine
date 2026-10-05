#pragma once

#include "Gameplay/Walkers.h"

#include <glad/glad.h>

#include <vector>

class GpuChunkCache;

// Draws walkers (WalkerSystem) into the GPU chunk data with shaders/people/walkers.comp: each
// frame it erases last frame's figures, then draws the current ones. Figures only replace air and
// only walker voxels are erased, so the world is never damaged; drawing every frame also restores
// figures wiped by a chunk re-upload.
class WalkerRenderer {
public:
    bool Init();
    void Draw(const std::vector<WalkerFigure>& figures, const GpuChunkCache& cache);

private:
    void Dispatch(GLuint buffer, int count, int mode);

    GLuint m_Program = 0;
    GLuint m_Buffers[2] = { 0, 0 }; // Last frame's figures and this frame's, swapped every frame
    int m_Counts[2] = { 0, 0 };
    int m_Current = 0;
    GLint m_ModeLocation = -1, m_CountLocation = -1, m_PageTableLocation = -1, m_PoolBaseLocation = -1;
};
