#include "UI/BuildMenu.h"

#include "Economy/ProductionChains.h"
#include "Economy/Treasury.h"
#include "Gameplay/BuildTool.h"
#include "Simulation/BuildingTypes.h"

#include <RmlUi/Core.h>

#include <cstdio>

namespace {

constexpr int MAX_ENTRIES = 16;
constexpr int REQUEST_OFFSET = 3; // Keeps ROAD (-2) and NO_TYPE (-1) above the "no request" 0

} // namespace

bool BuildMenu::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("build_menu");
    if (!model) return false;
    if (Rml::StructHandle<Entry> entry = model.RegisterStruct<Entry>()) {
        entry.RegisterMember("type", &Entry::type);
        entry.RegisterMember("name", &Entry::name);
        entry.RegisterMember("hotkey", &Entry::hotkey);
        entry.RegisterMember("image", &Entry::image);
        entry.RegisterMember("coins", &Entry::coins);
        entry.RegisterMember("planks", &Entry::planks);
        entry.RegisterMember("selected", &Entry::selected);
    }
    model.RegisterArray<std::vector<Entry>>();
    model.Bind("tab", &m_Tab);
    model.Bind("entries", &m_Entries);
    model.Bind("any_selected", &m_AnySelected);
    model.Bind("status", &m_Status);
    model.Bind("status_kind", &m_StatusKind);
    model.Bind("hint", &m_Hint);
    model.BindEventCallback("set_tab", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_TabRequest = arguments[0].Get<int>();
    });
    model.BindEventCallback("select", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_SelectRequest = arguments[0].Get<int>() + REQUEST_OFFSET;
    });
    m_Model = model.GetModelHandle();
    m_Entries.reserve(MAX_ENTRIES);

    m_Document = context->LoadDocument("assets/ui/build_menu.rml");
    return m_Document != nullptr;
}

void BuildMenu::SetVisible(bool visible) {
    if (!m_Document || m_Document->IsVisible() == visible) return;
    if (visible) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    else m_Document->Hide();
}

void BuildMenu::ApplyRequests(BuildTool& tool) {
    if (m_TabRequest >= 0) tool.SetTab((BuildCategory)m_TabRequest);
    if (m_SelectRequest != 0) {
        // Clicking the selected building again drops the selection
        int type = m_SelectRequest - REQUEST_OFFSET;
        tool.SelectType(type == tool.SelectedType() ? BuildTool::NO_TYPE : type);
    }
    m_TabRequest = -1;
    m_SelectRequest = 0;
}

void BuildMenu::FillEntries(const BuildTool& tool) {
    m_Entries.clear(); // Within the reserved capacity
    char hotkey[8];
    int count = std::min(BuildTool::EntryCount(tool.Tab()), MAX_ENTRIES);
    for (int i = 0; i < count; i++) {
        Entry entry;
        entry.type = BuildTool::EntryAt(tool.Tab(), i);
        std::snprintf(hotkey, sizeof(hotkey), "%d", i + 1);
        entry.hotkey = hotkey;
        if (entry.type == BuildTool::ROAD) {
            entry.name = "Road";
            entry.image = "buildings/road.tga";
        } else {
            entry.name = BUILDING_TYPES[entry.type].name;
            entry.image = Rml::String("buildings/") + BUILDING_TYPES[entry.type].modelName + ".tga"; // tools/ui_icons/buildings.py
            entry.coins = BUILDING_COSTS[entry.type].coins;
            entry.planks = BUILDING_COSTS[entry.type].planks;
        }
        m_Entries.push_back(entry);
    }
}

void BuildMenu::Update(const BuildTool& tool, uint32_t buildingCount) {
    if ((int)tool.Tab() != m_Tab) {
        m_Tab = (int)tool.Tab();
        FillEntries(tool);
        m_Model.DirtyVariable("tab");
        m_Model.DirtyVariable("entries");
    }
    int selected = tool.SelectedType();
    for (Entry& entry : m_Entries) {
        if (entry.selected == (entry.type == selected)) continue;
        entry.selected = entry.type == selected;
        m_Model.DirtyVariable("entries");
    }

    // Status line: what the current spot is like, and the controls
    char text[160];
    int kind = 0;
    const char* hint = "";
    if (selected == BuildTool::ROAD) {
        std::snprintf(text, sizeof(text), "Drag to build road");
        hint = "Right-drag removes road | right click cancels";
    } else if (selected != BuildTool::NO_TYPE) {
        hint = "R rotates | left click places | right click cancels";
        PlacementError error = tool.LastError();
        BuildingRole role = BUILDING_TYPES[selected].role;
        if (!tool.HasPlacementPreview()) {
            std::snprintf(text, sizeof(text), "Point at an island");
        } else if (error != PlacementError::None) {
            std::snprintf(text, sizeof(text), "%s", PlacementErrorText(error));
            kind = 3;
        } else if (tool.HasLocationPreview() && tool.PreviewLocation().needed > 0) {
            // Producers with a trees or pasture rule: productivity from the surroundings
            const LocationReport& location = tool.PreviewLocation();
            const char* what = PRODUCTION_CHAINS[BUILDING_TYPES[selected].chain].rule == LocationRule::Trees ? "trees" : "pasture";
            std::snprintf(text, sizeof(text), "Productivity %d%% (%d/%d %s)%s", location.factor / 10, location.count, location.needed, what,
                tool.PreviewConnected() ? "" : ", no road to a warehouse");
            kind = location.factor >= 1000 ? 1 : (location.factor >= 500 ? 2 : 3);
        } else if (role != BuildingRole::Storage && !tool.PreviewConnected()) {
            std::snprintf(text, sizeof(text), "OK, but no road to a warehouse here");
            kind = 2;
        } else if (role == BuildingRole::Residence && !tool.PreviewInMarketRange()) {
            std::snprintf(text, sizeof(text), "OK, but no marketplace in reach: nobody will move in");
            kind = 2;
        } else {
            std::snprintf(text, sizeof(text), "OK");
            kind = 1;
        }
    } else {
        std::snprintf(text, sizeof(text), "Buildings: %u", buildingCount);
        hint = "Right click demolishes | hold left click and drag to move";
    }
    if (m_Status != text) {
        m_Status = text;
        m_Model.DirtyVariable("status");
    }
    if (m_Hint != hint) {
        m_Hint = hint;
        m_Model.DirtyVariable("hint");
    }
    if (m_StatusKind != kind) {
        m_StatusKind = kind;
        m_Model.DirtyVariable("status_kind");
    }
    if (m_AnySelected != (selected != BuildTool::NO_TYPE)) {
        m_AnySelected = selected != BuildTool::NO_TYPE;
        m_Model.DirtyVariable("any_selected");
    }
}
