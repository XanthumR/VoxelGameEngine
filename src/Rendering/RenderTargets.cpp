#include "Rendering/RenderTargets.h"

namespace {

void AllocateTexture2D(GLuint& texture, GLenum internalFormat, int width, int height, GLenum filter) {
    if (texture) glDeleteTextures(1, &texture);
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
}

} // namespace

void RenderTargets::Init() {
    glGenFramebuffers(1, &m_Framebuffer);
}

void RenderTargets::Resize(int width, int height) {
    m_Width = width;
    m_Height = height;
    int halfWidth = (width + 1) / 2, halfHeight = (height + 1) / 2;
    AllocateTexture2D(m_Color, GL_RGBA32F, width, height, GL_LINEAR);
    AllocateTexture2D(m_GBuffer, GL_RGBA32I, width, height, GL_NEAREST);
    for (int i = 0; i < 2; i++) {
        AllocateTexture2D(m_ShadowValue[i], GL_RG32F, halfWidth, halfHeight, GL_NEAREST);
        AllocateTexture2D(m_ShadowKey[i], GL_RGBA32I, halfWidth, halfHeight, GL_NEAREST);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_Framebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Color, 0);
}

void RenderTargets::BindImages() const {
    glBindImageTexture(0, m_Color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(2, m_GBuffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32I);
    int previous = m_ShadowCurrent ^ 1;
    glBindImageTexture(3, m_ShadowValue[m_ShadowCurrent], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
    glBindImageTexture(4, m_ShadowKey[m_ShadowCurrent], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32I);
    glBindImageTexture(5, m_ShadowValue[previous], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
    glBindImageTexture(6, m_ShadowKey[previous], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32I);
}

// Stretches the (possibly smaller) render target to the window
void RenderTargets::BlitToWindow(int windowWidth, int windowHeight) const {
    bool scaled = m_Width != windowWidth || m_Height != windowHeight;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_Framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, m_Width, m_Height, 0, 0, windowWidth, windowHeight, GL_COLOR_BUFFER_BIT, scaled ? GL_LINEAR : GL_NEAREST);
}

double RenderTargets::MemoryBytes() const {
    double full = (double)m_Width * m_Height;
    double half = (double)((m_Width + 1) / 2) * ((m_Height + 1) / 2);
    return full * (16.0 + 16.0) + half * (8.0 + 16.0) * 2.0;
}
