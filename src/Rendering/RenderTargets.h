#pragma once

#include <glad/glad.h>

// The textures the render passes write, at the render resolution (window size * render scale),
// and the framebuffer used to stretch the final image onto the window
class RenderTargets {
public:
    void Init();
    void Resize(int width, int height); // Reallocates everything at the new size

    // Final color on image unit 0, G-buffer on 2, half-res shadow value and key on 3 and 4 (this
    // frame's) and 5 and 6 (last frame's, for the shadow pass to reuse)
    void BindImages() const;
    void SwapShadowHistory() { m_ShadowCurrent ^= 1; } // Last frame's shadows become the history
    void BlitToWindow(int windowWidth, int windowHeight) const;

    int Width() const { return m_Width; }
    int Height() const { return m_Height; }
    double MemoryBytes() const;

private:
    GLuint m_Framebuffer = 0;
    GLuint m_Color = 0;       // RGBA32F final image
    GLuint m_GBuffer = 0;     // RGBA32I: hit voxel xyz + packed normal/voxel ID/grid factor
    GLuint m_ShadowValue[2] = {}; // RG32F, half resolution: shadow, frames since it was traced
    GLuint m_ShadowKey[2] = {};   // RGBA32I, half resolution: voxel + normal each shadow was computed for
    int m_ShadowCurrent = 0;
    int m_Width = 0, m_Height = 0;
};
