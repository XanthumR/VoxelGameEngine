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
// (assets/ui/building_tooltip.rml: a house's residents, needs and upgrade progress; the houses a
// service building serves; a warehouse's island goods; a producer's status, productivity, buffers
// and cart; a farm's modules; taxes or upkeep), and the object menu of the clicked one docked at the
// bottom middle, as in Anno 1800 (assets/ui/object_menu.rml): its picture, name and status with
// Move, Copy, Pause (producers) and Demolish; a big figure (residents, productivity with its last
// 10 minutes as bars, storage capacity, residents served); a grid of slots (a house's needs, a
// producer's inputs and output, the island inventory); details; and the Upgrade, Build module and
// Build ship buttons. Its clicks are requests the application takes each frame.
class BuildingInfo {
public:
    struct Action {
        Rml::String label;
        bool enabled = false;
        int id = 0;
        bool operator==(const Action&) const = default;
    };

    struct Slot {
        Rml::String icon, label, value;
        float fill = -1.0f; // A bar under it, -1 for none
        int tone = 0;
        bool operator==(const Slot&) const = default;
    };
    struct Bar {
        Rml::String height; // "25dp"
        int percent = 0;
        bool operator==(const Bar&) const = default;
    };

    bool Init(Rml::Context* context);
    // tooltip: the hovered building, or INVALID for none; cursor in framebuffer pixels
    void Update(GameObjectId tooltip, GameObjectId panel, const Simulation& simulation, glm::vec2 cursor, glm::ivec2 size);
    void ApplyRequests(Simulation& simulation, BuildTool& tool);

private:
    enum ActionId { UPGRADE = 1, BUILD_SHIP = 2, BUILD_MODULE = 3 };
    enum ToolId { TOOL_MOVE = 1, TOOL_COPY = 2, TOOL_PAUSE = 3, TOOL_DEMOLISH = 4 };

    // The object menu's single values
    struct Menu {
        Rml::String title, subtitle, image, bigLabel, bigValue, bigUnit, slotsTitle;
        int subtitleTone = 0;
        float bigBar = -1.0f;
        bool canPause = false, paused = false;
    };
    void FillMenu(GameObjectId id, const Simulation& simulation);

    Rml::ElementDocument* m_Tooltip = nullptr;
    Rml::ElementDocument* m_Panel = nullptr;
    Rml::DataModelHandle m_TooltipModel, m_MenuModel;
    Rml::String m_TooltipTitle;
    InfoLines m_TooltipLines, m_MenuLines;
    Menu m_Menu;
    std::vector<Slot> m_Slots, m_SlotScratch;
    std::vector<Bar> m_History, m_HistoryScratch;
    std::vector<Action> m_Actions;
    GameObjectId m_PanelBuilding = INVALID_GAME_OBJECT;

    int m_ActionRequest = 0;
    int m_ToolRequest = 0;
    bool m_CloseRequest = false;
};
