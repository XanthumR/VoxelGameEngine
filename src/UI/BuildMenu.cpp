#include "UI/BuildMenu.h"

#include "Economy/ProductionChains.h"
#include "Economy/Treasury.h"
#include "Economy/IslandEconomy.h"
#include "Gameplay/BuildTool.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace {

constexpr int MAX_ENTRIES = 20; // Per group
constexpr int GROUP_COUNT = 3;
constexpr std::array<const char*, GROUP_COUNT> GROUP_NAMES = { "Houses and public buildings", "Production", "Trade and roads" };
constexpr std::array<const char*, (int)BuildCategory::Count> TAB_NAMES = { "Farmers", "Workers", "Artisans", "Infrastructure" };
constexpr std::array<const char*, (int)BuildCategory::Count> TAB_ICONS = { "icons/farmer.tga", "icons/worker.tga", "icons/artisan.tga",
    "icons/construction.tga" };
constexpr int REQUEST_OFFSET = 6; // Keeps the tools (-3 to -5), ROAD (-2) and NO_TYPE (-1) above the "no request" 0
constexpr const char* COST_NAMES[4] = { "coins", "planks", "bricks", "steel_beams" };

// The group a build menu entry goes in
int GroupOf(int type) {
    if (type == BuildTool::ROAD) return 2;
    switch (BUILDING_TYPES[type].role) {
    case BuildingRole::Residence:
    case BuildingRole::Service: return 0;
    case BuildingRole::Storage: return 2;
    default: return 1;
    }
}

// "Farmers, Workers" for the tiers that have a need
void AppendNeededBy(char* text, size_t size, bool good, ItemType item, ServiceType service) {
    int written = 0;
    text[0] = 0;
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        const PopulationTier& definition = POPULATION_TIERS[tier];
        for (int n = 0; n < definition.needCount; n++) {
            const Need& need = definition.needs[n];
            bool match = good ? need.kind == NeedKind::Good && need.item == item : need.kind == NeedKind::Service && need.service == service;
            if (!match) continue;
            written += std::snprintf(text + written, size - (size_t)written, "%s%s", written > 0 ? ", " : "", definition.name);
            if (written >= (int)size) return;
            break;
        }
    }
}

// A building's card for the construction menu: what it costs, what it does, who needs it
void BuildingCard(InfoLines& lines, int type, bool locked) {
    char text[128];
    if (type == BuildTool::ROAD) {
        lines.Add("Connects buildings to the warehouses and the marketplaces.");
        lines.Add("Drag to build, right-drag removes. Free.", Tone::Muted);
        return;
    }
    const BuildingType& building = BUILDING_TYPES[type];
    const BuildingCost& cost = BUILDING_COSTS[type];
    if (locked) {
        std::snprintf(text, sizeof(text), "Unlocks with the first %s", POPULATION_TIERS[(int)building.category].name);
        lines.Add(text, Tone::Bad).icon = TierIcon((int)building.category);
    }

    // What it does
    switch (building.role) {
    case BuildingRole::Residence:
        std::snprintf(text, sizeof(text), "Home of up to %d %s", POPULATION_TIERS[building.tier].maxResidents, POPULATION_TIERS[building.tier].name);
        lines.Add(text).icon = TierIcon(building.tier);
        lines.Add("Needs a road to a warehouse and a marketplace in reach.", Tone::Muted);
        break;
    case BuildingRole::Service: {
        std::snprintf(text, sizeof(text), "Serves the houses within %d road tiles", SERVICE_ROAD_RANGE[(size_t)building.service]);
        lines.Add(text).icon = "icons/residents.tga";
        char tiers[96];
        AppendNeededBy(tiers, sizeof(tiers), false, ItemType::Wood, building.service);
        std::snprintf(text, sizeof(text), "Needed by %s", tiers);
        if (tiers[0]) lines.Add(text, Tone::Good);
        break;
    }
    case BuildingRole::Storage:
        std::snprintf(text, sizeof(text), "Island storage +%d per good", WAREHOUSE_CAPACITY);
        lines.Add(text).icon = "icons/planks.tga";
        std::snprintf(text, sizeof(text), "Reaches %d road tiles; carts unload here", WAREHOUSE_ROAD_RANGE);
        lines.Add(text, Tone::Muted);
        if (building.dockRows > 0) lines.Add("On the coast: ships are built, load and unload here", Tone::Muted);
        break;
    case BuildingRole::Producer: {
        const ProductionChain& chain = PRODUCTION_CHAINS[building.chain];
        if (chain.inputCount == 0) std::snprintf(text, sizeof(text), "Makes %s", ItemName(chain.output));
        else if (chain.inputCount == 1) std::snprintf(text, sizeof(text), "Makes %s from %s", ItemName(chain.output), ItemName(chain.inputs[0]));
        else std::snprintf(text, sizeof(text), "Makes %s from %s and %s", ItemName(chain.output), ItemName(chain.inputs[0]), ItemName(chain.inputs[1]));
        InfoLine& makes = lines.Add(text);
        makes.icon = ItemIcon(chain.output);
        std::snprintf(text, sizeof(text), "%d s", chain.cycleTicks / 10);
        makes.value = text;
        std::snprintf(text, sizeof(text), "Workforce: %d %s", chain.workforce, POPULATION_TIERS[chain.workforceTier].name);
        lines.Add(text).icon = TierIcon(chain.workforceTier);
        int module = ModuleTypeOf(type);
        if (chain.rule == LocationRule::Coast) lines.Add("Must stand by the coast", Tone::Warn);
        if (chain.rule == LocationRule::Trees) {
            std::snprintf(text, sizeof(text), "Needs %d trees within %d tiles for full speed", chain.fullSpeedCount, chain.radius);
            lines.Add(text, Tone::Warn);
        }
        if (module >= 0) {
            std::snprintf(text, sizeof(text), "Needs %d %s modules around it for full speed", chain.fullSpeedCount, BUILDING_TYPES[module].name);
            lines.Add(text, Tone::Warn);
        }
        // Who uses the output: the tiers that need it, the producers that make something of it
        char users[128];
        AppendNeededBy(users, sizeof(users), true, chain.output, ServiceType::Marketplace);
        int written = (int)std::strlen(users);
        for (int t = 0; t < (int)BUILDING_TYPES.size() && written < (int)sizeof(users) - 1; t++) {
            if (BUILDING_TYPES[t].role != BuildingRole::Producer) continue;
            const ProductionChain& other = PRODUCTION_CHAINS[BUILDING_TYPES[t].chain];
            for (int i = 0; i < other.inputCount; i++) {
                if (other.inputs[i] != chain.output) continue;
                written += std::snprintf(users + written, sizeof(users) - (size_t)written, "%s%s", written > 0 ? ", " : "", BUILDING_TYPES[t].name);
                break;
            }
        }
        for (int m = 0; m < MATERIAL_COUNT; m++) {
            if (MATERIALS[m] == chain.output && written < (int)sizeof(users) - 1) {
                written += std::snprintf(users + written, sizeof(users) - (size_t)written, "%sconstruction", written > 0 ? ", " : "");
            }
        }
        std::snprintf(text, sizeof(text), "Used by %s", users);
        if (users[0]) lines.Add(text, Tone::Good);
        break;
    }
    case BuildingRole::Module: break;
    }

    // Cost
    InfoLine& coins = lines.Add("Cost");
    coins.icon = "icons/coin.tga";
    coins.value = Rml::ToString((int)cost.coins);
    std::array<int, MATERIAL_COUNT> materials = cost.Materials();
    for (int m = 0; m < MATERIAL_COUNT; m++) {
        if (materials[m] <= 0) continue;
        InfoLine& line = lines.Add(ItemName(MATERIALS[m]));
        line.icon = ItemIcon(MATERIALS[m]);
        line.value = Rml::ToString(materials[m]);
    }
    InfoLine& upkeep = lines.Add("Upkeep");
    upkeep.icon = "icons/coin.tga";
    std::snprintf(text, sizeof(text), cost.upkeep > 0 ? "-%d / min" : "none", cost.upkeep);
    upkeep.value = text;
    std::snprintf(text, sizeof(text), "%d x %d tiles", building.footprintWidth, building.footprintDepth);
    lines.Add(text, Tone::Muted);
}

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
        entry.RegisterMember("locked", &Entry::locked);
        entry.RegisterMember("unlock", &Entry::unlock);
    }
    model.RegisterArray<std::vector<Entry>>();
    if (Rml::StructHandle<Group> group = model.RegisterStruct<Group>()) {
        group.RegisterMember("name", &Group::name);
        group.RegisterMember("entries", &Group::entries);
        group.RegisterMember("used", &Group::used);
    }
    model.RegisterArray<std::vector<Group>>();
    if (Rml::StructHandle<Tab> tab = model.RegisterStruct<Tab>()) {
        tab.RegisterMember("name", &Tab::name);
        tab.RegisterMember("icon", &Tab::icon);
        tab.RegisterMember("locked", &Tab::locked);
        tab.RegisterMember("fresh", &Tab::fresh);
    }
    model.RegisterArray<std::vector<Tab>>();
    model.Bind("tabs", &m_Tabs);
    model.Bind("tab", &m_Tab);
    model.Bind("groups", &m_Groups);
    model.Bind("open", &m_Open);
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
    model.BindEventCallback("hover", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_HoverType = arguments[0].Get<int>();
    });
    model.BindEventCallback("toggle_menu", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_ToggleRequest = true; });
    m_Model = model.GetModelHandle();
    m_Groups.resize(GROUP_COUNT);
    for (int g = 0; g < GROUP_COUNT; g++) {
        m_Groups[g].name = GROUP_NAMES[g];
        m_Groups[g].entries.reserve(MAX_ENTRIES);
    }
    for (size_t i = 0; i < TAB_NAMES.size(); i++) m_Tabs.push_back({ TAB_NAMES[i], TAB_ICONS[i], false, false });

    m_Document = context->LoadDocument("assets/ui/build_menu.rml");

    Rml::DataModelConstructor cost = context->CreateDataModel("cost_tag");
    if (!cost) return false;
    for (int i = 0; i < 4; i++) {
        cost.Bind(COST_NAMES[i], &m_Cost[i]);
        cost.Bind(Rml::String("lack_") + COST_NAMES[i], &m_Lacking[i]);
    }
    m_CostModel = cost.GetModelHandle();
    m_CostTag = context->LoadDocument("assets/ui/cost_tag.rml");

    Rml::DataModelConstructor card = context->CreateDataModel("build_tooltip");
    if (!card) return false;
    card.Bind("title", &m_CardTitle);
    m_CardLines.Bind(card, "lines");
    m_CardModel = card.GetModelHandle();
    m_Card = context->LoadDocument("assets/ui/build_tooltip.rml");
    return m_Document != nullptr && m_CostTag != nullptr && m_Card != nullptr;
}

void BuildMenu::SetVisible(bool visible) {
    if (!m_Document || m_Document->IsVisible() == visible) return;
    if (visible) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    else m_Document->Hide();
}

void BuildMenu::ApplyRequests(BuildTool& tool) {
    if (m_ToggleRequest) tool.ToggleMenu();
    m_ToggleRequest = false;
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
    for (Group& group : m_Groups) group.entries.clear(); // Within the reserved capacity
    char hotkey[8];
    int count = BuildTool::EntryCount(tool.Tab());
    for (int i = 0; i < count; i++) {
        Entry entry;
        entry.type = BuildTool::EntryAt(tool.Tab(), i);
        entry.locked = !tool.BuildingUnlocked(entry.type);
        if (entry.locked) entry.unlock = Rml::String("With the first ") + POPULATION_TIERS[(int)tool.Tab()].name;
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
        std::vector<Entry>& entries = m_Groups[GroupOf(entry.type)].entries;
        if (entries.size() < (size_t)MAX_ENTRIES) entries.push_back(entry);
    }
    for (Group& group : m_Groups) group.used = !group.entries.empty();
}

void BuildMenu::UpdateCard(glm::vec2 cursor, glm::ivec2 size, const BuildTool& tool) {
    bool visible = m_Document && m_Document->IsVisible() && m_Open && m_HoverType != NO_HOVER;
    if (m_Card->IsVisible() != visible) {
        if (visible) {
            m_Card->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
            m_Card->PullToFront(); // Over the menu
        } else {
            m_Card->Hide();
        }
    }
    if (!visible) return;
    if (m_CardType != m_HoverType) {
        m_CardType = m_HoverType;
        Rml::String title = m_CardType == BuildTool::ROAD ? "Road" : BUILDING_TYPES[m_CardType].name;
        if (m_CardTitle != title) {
            m_CardTitle = title;
            m_CardModel.DirtyVariable("title");
        }
    }
    m_CardLines.Begin();
    BuildingCard(m_CardLines, m_CardType, !tool.BuildingUnlocked(m_CardType));
    m_CardLines.End(m_CardModel);
    // Just above the menu, over the hovered building, kept on the screen
    Rml::Vector2f box = m_Card->GetBox().GetSize(Rml::BoxArea::Border);
    float menuTop = m_Document->GetAbsoluteOffset(Rml::BoxArea::Border).y;
    m_Card->SetProperty("left", Rml::ToString(std::clamp(cursor.x - box.x * 0.5f, 8.0f, std::max(8.0f, size.x - box.x - 8.0f))) + "px");
    m_Card->SetProperty("top", Rml::ToString(std::max(8.0f, menuTop - box.y - 8.0f)) + "px");
}

void BuildMenu::UpdateCostTag(const BuildTool& tool, const Simulation& simulation, glm::vec2 cursor, glm::ivec2 size) {
    int type = tool.SelectedType();
    bool visible = m_Document && m_Document->IsVisible() && type >= 0 && tool.HasPlacementPreview() && tool.MovingBuilding() == INVALID_GAME_OBJECT;
    if (m_CostTag->IsVisible() != visible) {
        if (visible) {
            m_CostTag->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
            m_CostTag->PullToFront();
        } else {
            m_CostTag->Hide();
        }
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
    UpdateCard(cursor, size, tool);
    uint32_t buildingCount = simulation.Objects().AliveCount();
    if (m_Open != tool.MenuOpen()) {
        m_Open = tool.MenuOpen();
        m_Model.DirtyVariable("open");
    }
    int toolSelected = tool.SelectedType() <= BuildTool::DEMOLISH ? tool.SelectedType() : 0;
    if (m_Tool != toolSelected) {
        m_Tool = toolSelected;
        m_Model.DirtyVariable("tool");
    }
    // Tabs: locked until their tier is reached; one just unlocked glows until it is opened
    bool refill = (int)tool.Tab() != m_Tab;
    for (int i = 0; i < (int)m_Tabs.size(); i++) {
        Tab& tab = m_Tabs[i];
        bool locked = !tool.TabUnlocked((BuildCategory)i);
        bool fresh = (tab.fresh || (tab.locked && !locked && m_Tab >= 0)) && i != (int)tool.Tab();
        if (tab.locked == locked && tab.fresh == fresh) continue;
        refill |= tab.locked != locked && i == (int)tool.Tab(); // Its buildings unlock
        tab.locked = locked;
        tab.fresh = fresh;
        m_Model.DirtyVariable("tabs");
    }
    if (refill) {
        m_Tab = (int)tool.Tab();
        FillEntries(tool);
        m_Model.DirtyVariable("tab");
        m_Model.DirtyVariable("groups");
    }
    int selected = tool.SelectedType();
    for (Group& group : m_Groups) {
        for (Entry& entry : group.entries) {
            if (entry.selected == (entry.type == selected)) continue;
            entry.selected = entry.type == selected;
            m_Model.DirtyVariable("groups");
        }
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
        hint = m_Open ? "Delete demolishes | M moves | C copies | B closes the menu" : "B opens the construction menu | Delete demolishes | M moves | C copies";
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
