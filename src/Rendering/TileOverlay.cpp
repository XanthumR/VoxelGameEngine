#include "Rendering/TileOverlay.h"

#include <algorithm>
#include <cstdlib>

void TileOverlay::Init() {
    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, SIZE, SIZE);
    // Integer textures must use nearest filtering to be complete
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_Dirty = true;
    Upload();
}

bool TileOverlay::Recenter(glm::ivec2 focusTile) {
    glm::ivec2 offset = focusTile - (m_Origin + glm::ivec2(SIZE / 2));
    if (m_Centered && std::abs(offset.x) < RECENTER_DISTANCE && std::abs(offset.y) < RECENTER_DISTANCE) return false;
    m_Origin = focusTile - glm::ivec2(SIZE / 2);
    m_Centered = true;
    Clear();
    return true;
}

void TileOverlay::Clear() {
    std::fill(m_Tiles.begin(), m_Tiles.end(), (uint8_t)NONE);
    m_Dirty = true;
}

void TileOverlay::Set(glm::ivec2 tile, uint8_t color) {
    glm::ivec2 local = tile - m_Origin;
    if (local.x < 0 || local.y < 0 || local.x >= SIZE || local.y >= SIZE) return;
    m_Tiles[(size_t)local.y * SIZE + local.x] = color;
    m_Dirty = true;
}

void TileOverlay::Upload() {
    if (!m_Dirty || !m_Texture) return;
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SIZE, SIZE, GL_RED_INTEGER, GL_UNSIGNED_BYTE, m_Tiles.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    m_Dirty = false;
}
