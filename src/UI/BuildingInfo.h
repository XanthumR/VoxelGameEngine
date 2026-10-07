#pragma once

#include "Simulation/GameObjects.h"
#include "UI/InfoLines.h"

#include <glm/glm.hpp>

class BuildTool;
class IslandEconomyManager;
class Simulation;
namespace Rml {
class Context;
class ElementDocument;
}

// What a building is doing, in the game UI: a tooltip next to the cursor for the hovered building
// (assets/ui/building_tooltip.rml) and the panel of the clicked one (assets/ui/building_panel.rml).
// Both list the same details: a house's residents, needs and upgrade progress; the houses a
// marketplace serves; a warehouse's island goods; a producer's status, productivity, buffers and
// cart; taxes or upkeep. The panel adds the Upgrade button for houses and Build ship for harbors.
// Its clicks are requests the application takes each frame.
class BuildingInfo {
public:
    struct Action {
        Rml::String label;
        bool enabled = false;
        int id = 0;
        bool operator==(const Action&) const = default;
    };

    bool Init(Rml::Context* context);
    // tooltip: the hovered building, or INVALID for none; cursor in framebuffer pixels
    void Update(GameObjectId tooltip, GameObjectId panel, const Simulation& simulation, glm::vec2 cursor, glm::ivec2 size);
    void ApplyRequests(Simulation& simulation, BuildTool& tool);

private:
    enum ActionId { UPGRADE = 1, BUILD_SHIP = 2 };

    Rml::ElementDocument* m_Tooltip = nullptr;
    Rml::ElementDocument* m_Panel = nullptr;
    Rml::DataModelHandle m_TooltipModel, m_PanelModel;
    Rml::String m_TooltipTitle, m_PanelTitle;
    InfoLines m_TooltipLines, m_PanelLines;
    std::vector<Action> m_Actions;
    GameObjectId m_PanelBuilding = INVALID_GAME_OBJECT;

    int m_ActionRequest = 0;
    bool m_CloseRequest = false;
};
