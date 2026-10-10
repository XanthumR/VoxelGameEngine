#pragma once

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

// The build bar at the bottom of the screen (assets/ui/build_menu.rml, strategy camera): tabs, one
// button per building of the open tab (cost and hotkey), the demolish, move and copy tools, a cancel
// button, and a status line with the controls and why the current spot is invalid; and while a
// building is being placed, its cost beside the cursor (assets/ui/cost_tag.rml), red where the
// island lacks it. Clicks are requests BuildTool takes each frame.
class BuildMenu {
public:
    struct Entry {
        int type = 0; // Building type, or BuildTool::ROAD
        Rml::String name, hotkey, image; // image: its picture, relative to assets/ui
        int coins = 0, planks = 0, bricks = 0, steelBeams = 0;
        bool selected = false;
    };
    struct Tab {
        Rml::String name, icon;
        bool locked = false;
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
    std::vector<Tab> m_Tabs;
    std::vector<Entry> m_Entries;
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

    int m_TabRequest = -1;
    int m_SelectRequest = 0; // 0 = none; otherwise type + 3 (the type may be ROAD = -2 or NO_TYPE = -1)
};
