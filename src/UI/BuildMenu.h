#pragma once

#include "UI/InfoLines.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class BuildTool;
class Simulation;
namespace Rml {
class Context;
class ElementDocument;
}

// The construction menu at the bottom of the screen (assets/ui/build_menu.rml, strategy camera), as
// in Anno 1800: B opens and closes it; a tab per tier, the open tab's buildings in groups (houses
// and public buildings, production, trade and roads) with their cost and hotkey, greyed with what
// unlocks them until their tier is reached; a tab just unlocked glows until it is opened. Hovering
// a building shows its card (assets/ui/build_tooltip.rml): cost, upkeep, workforce, what it makes
// from what, where it may stand and who needs it. Beside it the demolish, move and copy tools and
// cancel, and a status line with the controls and why the current spot is invalid. While a building
// is being placed, its cost follows the cursor (assets/ui/cost_tag.rml), red where the island lacks
// it. Clicks are requests BuildTool takes each frame.
class BuildMenu {
public:
    struct Entry {
        int type = 0; // Building type, or BuildTool::ROAD
        Rml::String name, hotkey, image; // image: its picture, relative to assets/ui
        int coins = 0, planks = 0, bricks = 0, steelBeams = 0;
        bool selected = false;
        bool locked = false;  // Its tier is not reached yet
        Rml::String unlock;   // What unlocks it
    };
    struct Group {
        Rml::String name;
        std::vector<Entry> entries;
        bool used = false;
    };
    struct Tab {
        Rml::String name, icon;
        bool locked = false;
        bool fresh = false; // Unlocked since it was last opened
    };

    bool Init(Rml::Context* context);
    void SetVisible(bool visible);
    void ApplyRequests(BuildTool& tool); // Last frame's clicks
    // cursor and size: framebuffer pixels
    void Update(const BuildTool& tool, const Simulation& simulation, glm::vec2 cursor, glm::ivec2 size);

private:
    void FillEntries(const BuildTool& tool);

    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;

    // Bound to the document
    int m_Tab = -1;
    bool m_Open = true;
    std::vector<Tab> m_Tabs;
    std::vector<Group> m_Groups; // Always GROUP_COUNT, unused ones hidden
    bool m_AnySelected = false;
    Rml::String m_Status, m_Hint;
    int m_StatusKind = 0; // 0 muted, 1 good, 2 warning, 3 error
    int m_Tool = 0;       // The selected tool (BuildTool::DEMOLISH, ...), else 0

    // The cost tag
    void UpdateCostTag(const BuildTool& tool, const Simulation& simulation, glm::vec2 cursor, glm::ivec2 size);
    Rml::ElementDocument* m_CostTag = nullptr;
    Rml::DataModelHandle m_CostModel;
    int m_Cost[4] = {};       // Coins, planks, bricks, steel beams
    bool m_Lacking[4] = {};   // The treasury or the island has too few

    // The card of the hovered building
    void UpdateCard(glm::vec2 cursor, glm::ivec2 size, const BuildTool& tool);
    Rml::ElementDocument* m_Card = nullptr;
    Rml::DataModelHandle m_CardModel;
    Rml::String m_CardTitle;
    InfoLines m_CardLines;
    int m_HoverType = NO_HOVER; // Set by the cards' mouse events
    int m_CardType = NO_HOVER;  // The type the card shows
    static constexpr int NO_HOVER = -100;

    int m_TabRequest = -1;
    int m_SelectRequest = 0; // 0 = none; otherwise type + REQUEST_OFFSET (the type may be a tool, ROAD or NO_TYPE)
    bool m_ToggleRequest = false;
};
