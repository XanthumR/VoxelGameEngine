#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// Per build tile highlight colors around the camera focus, drawn by the shade pass on the ground's
// top faces: road range, road path previews, warehouse reach. One byte per tile (a palette index,
// see tileOverlayColor in shaders/render/shade.comp). Filled on the CPU, uploaded only when changed.
class TileOverlay {
public:
    static constexpr int SIZE = 256;            // Tiles per side
    static constexpr int RECENTER_DISTANCE = 64; // Tiles from the center before the window moves

    // Palette indices
    enum Color : uint8_t {
        NONE = 0,
        ROAD_IN_RANGE = 1,     // Soft blue
        ROAD_OUT_OF_RANGE = 2, // Grey
        PREVIEW_ADD = 3,       // Green
        PREVIEW_INVALID = 4,   // Red
        PREVIEW_REMOVE = 5,    // Orange
        WAREHOUSE_REACH = 6,   // Bright blue: the selected or previewed warehouse's roads
        MARKET_IN_RANGE = 7,   // Soft amber: roads a marketplace reaches
        MARKET_REACH = 8,      // Bright amber: the selected or previewed marketplace's roads
        LOCATION_COUNTED = 9,  // Light green: pasture or trees that count for the previewed producer
    };

    void Init(int tileSize); // tileSize: world columns per tile
    int TileSize() const { return m_TileSize; }

    // Moves the window when focusTile is far from its center; true if it moved (contents cleared)
    bool Recenter(glm::ivec2 focusTile);

    void Clear();
    void Set(glm::ivec2 tile, uint8_t color); // Tiles outside the window are ignored
    void Upload();                            // Only if something changed

    GLuint Texture() const { return m_Texture; }
    glm::ivec2 Origin() const { return m_Origin; } // Tile of texel (0, 0)

private:
    GLuint m_Texture = 0;
    int m_TileSize = 1;
    std::vector<uint8_t> m_Tiles = std::vector<uint8_t>((size_t)SIZE * SIZE, NONE);
    glm::ivec2 m_Origin = glm::ivec2(0);
    bool m_Centered = false;
    bool m_Dirty = true;
};
