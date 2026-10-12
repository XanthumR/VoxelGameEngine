#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "World/Sessions.h"

#include <cstdint>
#include <vector>

class Simulation;
class StrategyCamera;
class TerrainGenerator;
namespace Rml {
class Context;
class ElementDocument;
}

// The minimap at the bottom right (assets/ui/minimap.rml), as in Anno 1800: the whole map of the
// session the camera is in, one pixel per build tile, from the terrain's tile map (sea, beach,
// land, cliff, so it needs no loaded voxels); the player's roads and buildings on them (refreshed
// every second); the ships; and the camera's view. A click on the map glides the camera there; the
// compass button turns the camera back to face north; the session button goes to the other
// session once it is open. The document leaves a see-through window where the map goes, and the
// map is drawn there with ImGui's background list, which goes under the game UI.
class Minimap {
public:
    static constexpr int MAP_TILES = SESSION_TILES; // The map's side in build tiles

    bool Init(Rml::Context* context);
    ~Minimap();
    // The terrain the maps are drawn from (before the first Update)
    void SetTerrain(TerrainGenerator& terrain) { m_Terrain = &terrain; }
    void SetVisible(bool visible);
    // Follows the camera's session, refreshes the buildings now and then and draws the map into its
    // window. otherOpen: the session button is shown.
    void Update(const Simulation& simulation, const StrategyCamera& camera, double now, bool otherOpen);

    bool TakeClick(glm::vec2& worldColumn); // Where on the map was clicked (world columns)
    bool TakeCompass();
    bool TakeSessionSwitch(int& session); // The session to go to

private:
    void Refresh(const Simulation& simulation);
    void ShowSession(int session);

    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    TerrainGenerator* m_Terrain = nullptr;
    GLuint m_Texture = 0;
    glm::ivec2 m_Origin{ 0 };        // The map's first tile
    int m_Session = -1;              // Shown
    int m_SwitchTo = -1;             // Clicked, not taken yet
    // Bound to the document
    Rml::String m_SessionName, m_OtherName;
    bool m_OtherOpen = false;
    std::vector<uint32_t> m_Base;    // The islands, RGBA
    std::vector<uint32_t> m_Pixels;  // ... with the player's roads and buildings
    double m_LastRefresh = -1.0;
    glm::vec2 m_ClickPixel{ -1.0f }; // Where in the map window it was clicked (fractions), -1 for none
    bool m_Compass = false;
};
