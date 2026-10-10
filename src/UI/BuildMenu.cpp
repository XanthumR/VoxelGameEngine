#include "UI/BuildMenu.h"

#include "Economy/ProductionChains.h"
#include "Economy/Treasury.h"
#include "Economy/IslandEconomy.h"
#include "Gameplay/BuildTool.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"

#include <RmlUi/Core.h>

#include <array>
#include <cstdio>

namespace {

constexpr int MAX_ENTRIES = 20;
constexpr std::array<const char*, (int)BuildCategory::Count> TAB_NAMES = { "Farmers", "Workers", "Artisans", "Infrastructure" };
constexpr std::array<const char*, (int)BuildCategory::Count> TAB_ICONS = { "icons/farmer.tga", "icons/worker.tga", "icons/artisan.tga",
    "icons/construction.tga" };
constexpr int REQUEST_OFFSET = 6; // Keeps the tools (-3 to -5), ROAD (-2) and NO_TYPE (-1) above the "no request" 0
constexpr const char* COST_NAMES[4] = { "coins", "planks", "bricks", "steel_beams" };

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
        entry.RegisterMember("bricks", &Entry::bricks);
        entry.RegisterMember("steel_beams", &Entry::steelBeams);
        entry.RegisterMember("selected", &Entry::selected);
    }
    model.RegisterArray<std::vector<Entry>>();
    if (Rml::StructHandle<Tab> tab = model.RegisterStruct<Tab>()) {
        tab.RegisterMember("name", &Tab::name);
        tab.RegisterMember("icon", &Tab::icon);
        tab.RegisterMember("locked", &Tab::locked);
    }
    model.RegisterArray<std::vector<Tab>>();
    model.Bind("tabs", &m_Tabs);
    model.Bind("tab", &m_Tab);
    model.Bind("entries", &m_Entries);
    model.Bind("any_selected", &m_AnySelected);
    model.Bind("status", &m_Status);
    model.Bind("status_kind", &m_StatusKind);
    model.Bind("hint", &m_Hint);
    model.Bind("tool", &m_Tool);
    model.BindEventCallback("set_tab", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_TabRequest = arguments[0].Get<int>();
    });
    model.BindEventCallback("select", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_SelectRequest = arguments[0].Get<int>() + REQUEST_OFFSET;
    });
    m_Model = model.GetModelHandle();
    m_Entries.reserve(MAX_ENTRIES);
    for (size_t i = 0; i < TAB_NAMES.size(); i++) m_Tabs.push_back({ TAB_NAMES[i], TAB_ICONS[i], false });

    m_Document = context->LoadDocument("assets/ui/build_menu.rml");

    Rml::DataModelConstructor cost = context->CreateDataModel("cost_tag");
    if (!cost) return false;
    for (int i = 0; i < 4; i++) {
        cost.Bind(COST_NAMES[i], &m_Cost[i]);
        cost.Bind(Rml::String("lack_") + COST_NAMES[i], &m_Lacking[i]);
    }
    m_CostModel = cost.GetModelHandle();
    m_CostTag = context->LoadDocument("assets/ui/cost_tag.rml");
    return m_Document != nullptr && m_CostTag != nullptr;
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
        if (i < 10) std::snprintf(hotkey, sizeof(hotkey), "%d", (i + 1) % 10);
        else hotkey[0] = 0;
        entry.hotkey = hotkey;
        if (entry.type == BuildTool::ROAD) {
            entry.name = "Road";
            entry.image = "buildings/road.tga";
        } else {
            entry.name = BUILDING_TYPES[entry.type].name;
            entry.image = Rml::String("buildings/") + BUILDING_TYPES[entry.type].modelName + ".tga"; // tools/ui_icons/buildings.py
            entry.coins = BUILDING_COSTS[entry.type].coins;
            entry.planks = BUILDING_COSTS[entry.type].planks;
            entry.bricks = BUILDING_COSTS[entry.type].bricks;
            entry.steelBeams = BUILDING_COSTS[entry.type].steelBeams;
        }
        m_Entries.push_back(entry);
    }
}

void BuildMenu::UpdateCostTag(const BuildTool& tool, const Simulation& simulation, glm::vec2 cursor, glm::ivec2 size) {
    int type = tool.SelectedType();
    bool visible = m_Document && m_Document->IsVisible() && type >= 0 && tool.HasPlacementPreview() && tool.MovingBuilding() == INVALID_GAME_OBJECT;
    if (m_CostTag->IsVisible() != visible) {
        if (visible) m_CostTag->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        else m_CostTag->Hide();
    }
    if (!visible) return;

    const BuildingCost& cost = BUILDING_COSTS[type];
    const IslandStorage* storage = simulation.Economy().Find(tool.PreviewIsland());
    std::array<int, MATERIAL_COUNT> materials = cost.Materials();
    int values[4] = { cost.coins, materials[0], materials[1], materials[2] };
    bool lacking[4] = { simulation.Coins().Coins() < cost.coins, false, false, false };
    for (int i = 0; i < MATERIAL_COUNT; i++) lacking[i + 1] = materials[i] > 0 && (!storage || storage->Amount(MATERIALS[i]) < materials[i]);
    for (int i = 0; i < 4; i++) {
        if (m_Cost[i] != values[i]) {
            m_Cost[i] = values[i];
            m_CostModel.DirtyVariable(COST_NAMES[i]);
        }
        if (m_Lacking[i] != lacking[i]) {
            m_Lacking[i] = lacking[i];
            m_CostModel.DirtyVariable(Rml::String("lack_") + COST_NAMES[i]);
        }
    }
    // Below and right of the cursor, kept on the screen
    Rml::Vector2f box = m_CostTag->GetBox().GetSize(Rml::BoxArea::Border);
    m_CostTag->SetProperty("left", Rml::ToString(std::max(0.0f, std::min(cursor.x + 24.0f, size.x - box.x - 8.0f))) + "px");
    m_CostTag->SetProperty("top", Rml::ToString(std::max(0.0f, std::min(cursor.y + 24.0f, size.y - box.y - 8.0f))) + "px");
}

void BuildMenu::Update(const BuildTool& tool, const Simulation& simulation, glm::vec2 cursor, glm::ivec2 size) {
    UpdateCostTag(tool, simulation, cursor, size);
    uint32_t buildingCount = simulation.Objects().AliveCount();
    int toolSelected = tool.SelectedType() <= BuildTool::DEMOLISH ? tool.SelectedType() : 0;
    if (m_Tool != toolSelected) {
        m_Tool = toolSelected;
        m_Model.DirtyVariable("tool");
    }
    for (int i = 0; i < (int)m_Tabs.size(); i++) {
        bool locked = !tool.TabUnlocked((BuildCategory)i);
        if (m_Tabs[i].locked == locked) continue;
        m_Tabs[i].locked = locked;
        m_Model.DirtyVariable("tabs");
    }
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
    if (tool.MovingBuilding() != INVALID_GAME_OBJECT) {
        std::snprintf(text, sizeof(text), "%s", tool.LastError() == PlacementError::None ? "Moving: free of charge" : PlacementErrorText(tool.LastError()));
        kind = tool.LastError() == PlacementError::None ? 1 : 3;
        hint = "R or Shift + wheel rotates | click sets it down | right click puts it back";
    } else if (selected == BuildTool::DEMOLISH) {
        std::snprintf(text, sizeof(text), "Demolish: click or drag over buildings and roads (half the cost back)");
        kind = 3;
        hint = "Delete or right click ends";
    } else if (selected == BuildTool::MOVE) {
        std::snprintf(text, sizeof(text), "Move: click a building to pick it up");
        hint = "M or right click ends";
    } else if (selected == BuildTool::COPY) {
        std::snprintf(text, sizeof(text), "Copy: click a building to build another like it");
        hint = "C or right click ends";
    } else if (selected == BuildTool::ROAD) {
        std::snprintf(text, sizeof(text), "Drag to build road");
        hint = "Right-drag removes road | right click cancels";
    } else if (selected != BuildTool::NO_TYPE) {
        hint = "R or Shift + wheel rotates | left click places | right click cancels";
        PlacementError error = tool.LastError();
        BuildingRole role = BUILDING_TYPES[selected].role;
        if (!tool.HasPlacementPreview()) {
            std::snprintf(text, sizeof(text), "Point at an island");
        } else if (error != PlacementError::None) {
            std::snprintf(text, sizeof(text), "%s", PlacementErrorText(error));
            kind = 3;
        } else if (role == BuildingRole::Module) {
            // A farm's pen: how many its farm has
            const LocationReport& location = tool.PreviewLocation();
            std::snprintf(text, sizeof(text), "OK: %s %d/%d for its %s", BUILDING_TYPES[selected].name, location.count, location.needed,
                BUILDING_TYPES[BUILDING_TYPES[selected].moduleOf].name);
            kind = 1;
        } else if (ModuleTypeOf(selected) >= 0) {
            std::snprintf(text, sizeof(text), "OK: then place its %s pens on the green tiles (%d for full speed)%s", BUILDING_TYPES[ModuleTypeOf(selected)].name,
                PRODUCTION_CHAINS[BUILDING_TYPES[selected].chain].fullSpeedCount, tool.PreviewConnected() ? "" : ", no road to a warehouse");
            kind = tool.PreviewConnected() ? 1 : 2;
        } else if (tool.HasLocationPreview() && tool.PreviewLocation().needed > 0) {
            // Producers with a trees rule: productivity from the surroundings
            const LocationReport& location = tool.PreviewLocation();
            std::snprintf(text, sizeof(text), "Productivity %d%% (%d/%d trees)%s", location.factor / 10, location.count, location.needed,
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
        hint = "Delete demolishes | M moves | C copies | hold left click and drag to move";
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
