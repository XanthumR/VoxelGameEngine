#include "UI/Statistics.h"

#include "Economy/IslandEconomy.h"
#include "Economy/ProductionChains.h"
#include "Economy/Treasury.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"
#include "UI/InfoLines.h"

#include <RmlUi/Core.h>

#include <array>
#include <cstdio>

namespace {

constexpr int MAX_ROWS = 64;

template <typename T>
void SetIfChanged(T& field, const T& value, Rml::DataModelHandle model, const char* name) {
    if (field == value) return;
    field = value;
    model.DirtyVariable(name);
}

Rml::String Signed(int value) {
    char text[16];
    std::snprintf(text, sizeof(text), "%+d", value);
    return text;
}

constexpr const char* COLUMN_NAMES[5] = { "col_a", "col_b", "col_c", "col_d", "col_e" };

int ToneOfBalance(int value) {
    return value > 0 ? (int)Tone::Good : value < 0 ? (int)Tone::Bad : (int)Tone::Muted;
}

} // namespace

bool Statistics::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("statistics");
    if (!model) return false;
    if (Rml::StructHandle<Row> row = model.RegisterStruct<Row>()) {
        row.RegisterMember("icon", &Row::icon);
        row.RegisterMember("name", &Row::name);
        row.RegisterMember("a", &Row::a);
        row.RegisterMember("b", &Row::b);
        row.RegisterMember("c", &Row::c);
        row.RegisterMember("d", &Row::d);
        row.RegisterMember("e", &Row::e);
        row.RegisterMember("tone", &Row::tone);
        row.RegisterMember("heading", &Row::heading);
    }
    model.RegisterArray<std::vector<Row>>();
    model.Bind("rows", &m_Rows);
    model.Bind("tab", &m_Tab);
    model.Bind("all_islands", &m_AllIslands);
    model.Bind("title", &m_Title);
    model.Bind("summary", &m_Summary);
    for (int i = 0; i < 5; i++) model.Bind(COLUMN_NAMES[i], &m_Columns[i]);
    model.BindEventCallback("set_tab", [this](Rml::DataModelHandle handle, Rml::Event&, const Rml::VariantList& arguments) {
        if (arguments.empty()) return;
        m_Tab = arguments[0].Get<int>();
        handle.DirtyVariable("tab");
    });
    model.BindEventCallback("toggle_islands", [this](Rml::DataModelHandle handle, Rml::Event&, const Rml::VariantList&) {
        m_AllIslands = !m_AllIslands;
        handle.DirtyVariable("all_islands");
    });
    model.BindEventCallback("close", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_Open = false; });
    m_Model = model.GetModelHandle();
    m_Rows.reserve(MAX_ROWS);
    m_Scratch.reserve(MAX_ROWS);
    m_Document = context->LoadDocument("assets/ui/statistics.rml");
    return m_Document != nullptr;
}

void Statistics::Update(const Simulation& simulation, IslandId island, bool visible) {
    visible = visible && m_Open;
    if (m_Document && m_Document->IsVisible() != visible) {
        if (visible) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        else m_Document->Hide();
    }
    if (!visible) return;

    SetIfChanged(m_Title, Rml::String(m_AllIslands ? "All islands" : IslandName(island)), m_Model, "title");
    m_Scratch.clear();
    if (m_Tab == 0) FillProduction(simulation, island);
    else if (m_Tab == 1) FillPopulation(simulation, island);
    else FillFinance(simulation, island);
    if (m_Rows != m_Scratch) {
        m_Rows = m_Scratch;
        m_Model.DirtyVariable("rows");
    }
}

void Statistics::SetColumns(const char* const (&columns)[5]) {
    for (int i = 0; i < 5; i++) SetIfChanged(m_Columns[i], Rml::String(columns[i]), m_Model, COLUMN_NAMES[i]);
}

void Statistics::FillProduction(const Simulation& simulation, IslandId island) {
    const char* const columns[5] = { "Producers", "Made / min", "Used / min", "Stock", "Balance" };
    SetColumns(columns);

    // Producers of each good on the island (or all)
    std::array<int, ITEM_COUNT> producers = {};
    const GameObjectRegistry& objects = simulation.Objects();
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
        if (type.role != BuildingRole::Producer || (!m_AllIslands && objects.Building(id).island != island)) continue;
        producers[(size_t)PRODUCTION_CHAINS[type.chain].output]++;
    }
    int shortages = 0;
    for (int i = 0; i < ITEM_COUNT; i++) {
        ItemType item = (ItemType)i;
        int made = 0, used = 0, stock = 0;
        const IslandEconomyManager& economy = simulation.Economy();
        for (size_t s = 0; s < economy.IslandSlotCount(); s++) {
            const IslandStorage& storage = economy.IslandAt(s);
            if (!m_AllIslands && storage.island != island) continue;
            made += storage.MadePerMinute(item);
            used += storage.UsedPerMinute(item);
            stock += storage.Amount(item);
        }
        if (made == 0 && used == 0 && stock == 0 && producers[i] == 0) continue;
        int balance = made - used;
        if (balance < 0) shortages++;
        m_Scratch.push_back({ ItemIcon(item), ITEM_NAMES[i], Rml::ToString(producers[i]), Rml::ToString(made), Rml::ToString(used), Rml::ToString(stock),
            Signed(balance), ToneOfBalance(balance), false }); // Within the reserve
    }
    char text[96];
    std::snprintf(text, sizeof(text), shortages > 0 ? "%d goods are used faster than they are made" : "Every good is made at least as fast as it is used",
        shortages);
    SetIfChanged(m_Summary, Rml::String(m_Scratch.empty() ? "Nothing made or stored yet" : text), m_Model, "summary");
}

void Statistics::FillPopulation(const Simulation& simulation, IslandId island) {
    const char* const columns[5] = { "Residents", "Houses", "Jobs", "Filled", "Needs met" };
    SetColumns(columns);

    // Houses and their needs, weighted by residents
    const GameObjectRegistry& objects = simulation.Objects();
    int houses[TIER_COUNT] = {};
    int64_t supply[TIER_COUNT][MAX_NEEDS] = {}, weight[TIER_COUNT] = {};
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
        if (type.role != BuildingRole::Residence || (!m_AllIslands && objects.Building(id).island != island)) continue;
        const ResidenceComponent& residence = objects.Residence(id);
        int residents = residence.residents > 0 ? residence.residents : 1;
        houses[type.tier]++;
        weight[type.tier] += residents;
        for (int n = 0; n < POPULATION_TIERS[type.tier].needCount; n++) supply[type.tier][n] += (int64_t)residence.needSupply[n] * residents;
    }
    const IslandEconomyManager& economy = simulation.Economy();
    int total = 0;
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        int residents = 0, jobs = 0, filledJobs = 0;
        for (size_t s = 0; s < economy.IslandSlotCount(); s++) {
            const IslandStorage& storage = economy.IslandAt(s);
            if (!m_AllIslands && storage.island != island) continue;
            residents += storage.population[tier];
            jobs += storage.jobs[tier];
            filledJobs += storage.jobs[tier] * storage.workforce[tier] / 1000;
        }
        if (houses[tier] == 0 && jobs == 0) continue;
        total += residents;
        const PopulationTier& definition = POPULATION_TIERS[tier];
        int met = 0;
        for (int n = 0; n < definition.needCount; n++) met += weight[tier] > 0 ? (int)(supply[tier][n] / weight[tier]) : 0;
        met = definition.needCount > 0 ? met / definition.needCount : 0;
        char filled[16], metText[16];
        std::snprintf(filled, sizeof(filled), jobs > 0 ? "%d%%" : "-", jobs > 0 ? filledJobs * 100 / jobs : 0);
        std::snprintf(metText, sizeof(metText), "%d%%", met / 10);
        m_Scratch.push_back({ TierIcon(tier), definition.name, Rml::ToString(residents), Rml::ToString(houses[tier]), Rml::ToString(jobs), filled, metText,
            met >= 900 ? (int)Tone::Good : met >= 500 ? (int)Tone::Warn : (int)Tone::Bad, true });
        for (int n = 0; n < definition.needCount; n++) {
            int perMille = weight[tier] > 0 ? (int)(supply[tier][n] / weight[tier]) : 0;
            char percent[16];
            std::snprintf(percent, sizeof(percent), "%d%%", perMille / 10);
            m_Scratch.push_back({ NeedIcon(definition.needs[n]), NeedName(definition.needs[n]), "", "", "", "", percent,
                perMille >= 900 ? (int)Tone::Good : perMille >= 500 ? (int)Tone::Warn : (int)Tone::Bad, false });
        }
    }
    char text[64];
    std::snprintf(text, sizeof(text), "%d residents", total);
    SetIfChanged(m_Summary, Rml::String(text), m_Model, "summary");
}

void Statistics::FillFinance(const Simulation& simulation, IslandId island) {
    const char* const columns[5] = { "Count", "", "", "", "Per minute" };
    SetColumns(columns);

    const GameObjectRegistry& objects = simulation.Objects();
    int64_t taxes[TIER_COUNT] = {};
    int houses[TIER_COUNT] = {};
    std::array<int, BUILDING_TYPES.size()> counts = {};
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || (!m_AllIslands && objects.Building(id).island != island)) continue;
        uint16_t type = objects.Building(id).type;
        counts[type]++;
        if (BUILDING_TYPES[type].role == BuildingRole::Residence) {
            taxes[BUILDING_TYPES[type].tier] += Treasury::HouseTaxMilli(objects, id);
            houses[BUILDING_TYPES[type].tier]++;
        }
    }
    int64_t income = 0, upkeep = 0;
    m_Scratch.push_back({ "icons/coin.tga", "Taxes", "", "", "", "", "", 0, true });
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        if (houses[tier] == 0) continue;
        income += taxes[tier];
        m_Scratch.push_back({ TierIcon(tier), POPULATION_TIERS[tier].name, Rml::ToString(houses[tier]), "", "", "", Signed((int)(taxes[tier] / 1000)),
            (int)Tone::Good, false });
    }
    m_Scratch.push_back({ "icons/coin.tga", "Upkeep", "", "", "", "", "", 0, true });
    for (size_t type = 0; type < BUILDING_TYPES.size(); type++) {
        int cost = BUILDING_COSTS[type].upkeep * counts[type];
        if (cost == 0) continue;
        upkeep += cost;
        m_Scratch.push_back({ "", BUILDING_TYPES[type].name, Rml::ToString(counts[type]), "", "", "", Signed(-cost), (int)Tone::Bad, false });
        if (m_Scratch.size() >= (size_t)MAX_ROWS - 1) break;
    }
    int net = (int)(income / 1000 - upkeep);
    char text[96];
    std::snprintf(text, sizeof(text), "Balance %+d / min (taxes %+d, upkeep %d)", net, (int)(income / 1000), (int)-upkeep);
    SetIfChanged(m_Summary, Rml::String(text), m_Model, "summary");
}
