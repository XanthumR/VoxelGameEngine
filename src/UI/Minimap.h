#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class Simulation;
class StrategyCamera;
class TerrainGenerator;
namespace Rml {
class Context;
class ElementDocument;
}

// The minimap at the bottom right (assets/ui/minimap.rml), as in Anno 1800: the islands around the
// spawn, one pixel per build tile, from the terrain's tile map (sea, beach, land, cliff, so it needs
// no loaded voxels); the player's roads and buildings on them (refreshed every second); the ships;
// and the camera's view. A click on the map glides the camera there; the compass button turns the
// camera back to face north. The document leaves a see-through window where the map goes, and the
// map is drawn there with ImGui's background list, which goes under the game UI.
class Minimap {
public:
    static constexpr int MAP_TILES = 384; // The map's side in build tiles (4608 voxel columns)

    bool Init(Rml::Context* context);
    ~Minimap();
    // Draws the islands around this tile (once, after the spawn is known)
    void SetCenter(TerrainGenerator& terrain, glm::ivec2 centerTile);
    void SetVisible(bool visible);
    // Refreshes the buildings now and then and draws the map into its window
    void Update(const Simulation& simulation, const StrategyCamera& camera, double now);

    bool TakeClick(glm::vec2& worldColumn); // Where on the map was clicked (world columns)
    bool TakeCompass();

private:
    void Refresh(const Simulation& simulation);

    Rml::ElementDocument* m_Document = nullptr;
    GLuint m_Texture = 0;
    glm::ivec2 m_Origin{ 0 };        // The map's first tile
    bool m_HasCenter = false;
    std::vector<uint32_t> m_Base;    // The islands, RGBA
    std::vector<uint32_t> m_Pixels;  // ... with the player's roads and buildings
    double m_LastRefresh = -1.0;
    glm::vec2 m_ClickPixel{ -1.0f }; // Where in the map window it was clicked (fractions), -1 for none
    bool m_Compass = false;
};
