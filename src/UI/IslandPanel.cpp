#include "UI/IslandPanel.h"

#include "Economy/IslandEconomy.h"
#include "Economy/Treasury.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cstdio>

namespace {

void Show(Rml::ElementDocument* document, bool visible) {
    if (!document || document->IsVisible() == visible) return;
    if (visible) document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    else document->Hide();
}

template <typename T>
void SetIfChanged(T& field, const T& value, Rml::DataModelHandle model, const char* name) {
    if (field == value) return;
    field = value;
    model.DirtyVariable(name);
}

// The inventory's filters: 1 what residents consume, 2 construction materials, 3 the rest
int KindOf(ItemType item) {
    for (ItemType material : MATERIALS) {
        if (material == item) return 2;
    }
    for (const PopulationTier& tier : POPULATION_TIERS) {
        for (int n = 0; n < tier.needCount; n++) {
            if (tier.needs[n].kind == NeedKind::Good && tier.needs[n].item == item) return 1;
        }
    }
    return 3;
}

} // namespace

bool IslandPanel::Init(Rml::Context* context) {
    Rml::DataModelConstructor bar = context->CreateDataModel("island_bar");
    if (!bar) return false;
    bar.Bind("name", &m_Name);
    bar.Bind("note", &m_Note);
    bar.Bind("settled", &m_Settled);
    bar.Bind("details_open", &m_DetailsOpen);
    bar.Bind("inventory_open", &m_InventoryOpen);
    if (Rml::StructHandle<Tier> tier = bar.RegisterStruct<Tier>()) {
        tier.RegisterMember("icon", &Tier::icon);
        tier.RegisterMember("residents", &Tier::residents);
    }
    bar.RegisterArray<std::vector<Tier>>();
    bar.Bind("tiers", &m_Tiers);
    bar.BindEventCallback("toggle_details", [this](Rml::DataModelHandle model, Rml::Event&, const Rml::VariantList&) {
        m_DetailsOpen = !m_DetailsOpen;
        model.DirtyVariable("details_open");
    });
    bar.BindEventCallback("toggle_inventory", [this](Rml::DataModelHandle model, Rml::Event&, const Rml::VariantList&) {
        m_InventoryOpen = !m_InventoryOpen;
        model.DirtyVariable("inventory_open");
    });
    m_BarModel = bar.GetModelHandle();
    m_Tiers.reserve(TIER_COUNT);
    m_TierScratch.reserve(TIER_COUNT);

    Rml::DataModelConstructor model = context->CreateDataModel("island_panel");
    if (!model) return false;
    model.Bind("title", &m_Title);
    m_Lines.Bind(model, "lines");
    model.BindEventCallback("close", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        m_DetailsOpen = false;
        m_BarModel.DirtyVariable("details_open");
    });
    m_Model = model.GetModelHandle();

    Rml::DataModelConstructor inventory = context->CreateDataModel("inventory");
    if (!inventory) return false;
    inventory.Bind("title", &m_InventoryTitle);
    inventory.Bind("capacity", &m_Capacity);
    inventory.Bind("filter", &m_Filter);
    if (Rml::StructHandle<Good> good = inventory.RegisterStruct<Good>()) {
        good.RegisterMember("icon", &Good::icon);
        good.RegisterMember("name", &Good::name);
        good.RegisterMember("amount", &Good::amount);
        good.RegisterMember("trend", &Good::trend);
        good.RegisterMember("trend_icon", &Good::trendIcon);
        good.RegisterMember("trend_tone", &Good::trendTone);
        good.RegisterMember("fill", &Good::fill);
    }
    inventory.RegisterArray<std::vector<Good>>();
    inventory.Bind("goods", &m_Goods);
    inventory.BindEventCallback("set_filter", [this](Rml::DataModelHandle model, Rml::Event&, const Rml::VariantList& arguments) {
        if (arguments.empty()) return;
        m_Filter = arguments[0].Get<int>();
        model.DirtyVariable("filter");
    });
    inventory.BindEventCallback("close", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        m_InventoryOpen = false;
        m_BarModel.DirtyVariable("inventory_open");
    });
    m_InventoryModel = inventory.GetModelHandle();
    m_Goods.reserve(ITEM_COUNT);
    m_GoodScratch.reserve(ITEM_COUNT);

    m_Bar = context->LoadDocument("assets/ui/island_bar.rml");
    m_Document = context->LoadDocument("assets/ui/island_panel.rml");
    m_Inventory = context->LoadDocument("assets/ui/inventory.rml");
    return m_Bar && m_Document && m_Inventory;
}

void IslandPanel::Update(IslandId island, const IslandEconomyManager& economy, bool visible) {
    visible = visible && island != NO_ISLAND;
    const IslandStorage* storage = economy.Find(island);
    bool settled = storage && storage->warehouseCount > 0;
    Show(m_Bar, visible);
    Show(m_Document, visible && m_DetailsOpen);
    Show(m_Inventory, visible && m_InventoryOpen && settled);
    if (!visible) return;
    UpdateBar(island, economy);
    if (m_DetailsOpen) UpdateDetails(island, economy);
    if (m_InventoryOpen && settled) UpdateInventory(island, economy);
}

void IslandPanel::UpdateBar(IslandId island, const IslandEconomyManager& economy) {
    const IslandStorage* storage = economy.Find(island);
    bool settled = storage && storage->warehouseCount > 0;
    SetIfChanged(m_Name, Rml::String(IslandName(island)), m_BarModel, "name");
    SetIfChanged(m_Settled, settled, m_BarModel, "settled");
    SetIfChanged(m_Note, Rml::String(settled ? "" : "Not settled"), m_BarModel, "note");
    // Its residents, the tiers it has
    m_TierScratch.clear();
    for (int tier = 0; settled && tier < TIER_COUNT; tier++) {
        if (tier > 0 && storage->population[tier] == 0) continue;
        m_TierScratch.push_back({ TierIcon(tier), storage->population[tier] }); // Within the reserve
    }
    if (m_Tiers != m_TierScratch) {
        m_Tiers = m_TierScratch;
        m_BarModel.DirtyVariable("tiers");
    }
}

void IslandPanel::UpdateInventory(IslandId island, const IslandEconomyManager& economy) {
    const IslandStorage& storage = *economy.Find(island);
    char text[32];
    std::snprintf(text, sizeof(text), "%d per good", storage.CapacityPerItem());
    SetIfChanged(m_Capacity, Rml::String(text), m_InventoryModel, "capacity");
    SetIfChanged(m_InventoryTitle, Rml::String(IslandName(island)), m_InventoryModel, "title");
    m_GoodScratch.clear();
    for (int i = 0; i < ITEM_COUNT; i++) {
        ItemType item = (ItemType)i;
        int trend = storage.TrendPerMinute(item);
        if (storage.amounts[i] == 0 && trend == 0) continue; // Not here, and not just used up
        if (m_Filter != 0 && KindOf(item) != m_Filter) continue;
        Good good;
        good.icon = ItemIcon(item);
        good.name = ITEM_NAMES[i];
        good.amount = Rml::ToString(storage.amounts[i]);
        if (trend != 0) std::snprintf(text, sizeof(text), "%+d / min", trend);
        else std::snprintf(text, sizeof(text), "steady");
        good.trend = text;
        good.trendIcon = trend > 0 ? "icons/trend_up.tga" : trend < 0 ? "icons/trend_down.tga" : "";
        good.trendTone = trend > 0 ? (int)Tone::Good : trend < 0 ? (int)Tone::Bad : (int)Tone::Muted;
        good.fill = std::min(1.0f, (float)storage.amounts[i] / std::max(1, storage.CapacityPerItem()));
        m_GoodScratch.push_back(good); // Within the reserve
    }
    if (m_Goods != m_GoodScratch) {
        m_Goods = m_GoodScratch;
        m_InventoryModel.DirtyVariable("goods");
    }
}

void IslandPanel::UpdateDetails(IslandId island, const IslandEconomyManager& economy) {
    char text[96];
    SetIfChanged(m_Title, Rml::String(IslandName(island)), m_Model, "title");

    m_Lines.Begin();
    const IslandStorage* storage = economy.Find(island);
    if (!storage || storage->warehouseCount == 0) {
        m_Lines.Add("Not settled", Tone::Heading);
        if (economy.SettledIslandCount() == 0 || storage) {
            m_Lines.Add("Build a warehouse to settle it.", Tone::Muted);
        } else {
            m_Lines.Add("Anchor a ship within 4 tiles of the coast,", Tone::Muted);
            m_Lines.Add("then build a warehouse or a harbor:", Tone::Muted);
            m_Lines.Add("its planks come from the ship.", Tone::Muted);
        }
        m_Lines.End(m_Model);
        return;
    }

    // Population, tier by tier: jobs, then the supply of each good need
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        const PopulationTier& definition = POPULATION_TIERS[tier];
        if (tier > 0 && storage->population[tier] == 0 && storage->jobs[tier] == 0) continue; // Upper tiers once they matter
        InfoLine& heading = m_Lines.Add(definition.name, Tone::Heading);
        heading.icon = TierIcon(tier);
        heading.value = Rml::ToString(storage->population[tier]);
        if (storage->jobs[tier] > 0) {
            std::snprintf(text, sizeof(text), "Jobs %d, %d%% filled", storage->jobs[tier], storage->workforce[tier] / 10);
            m_Lines.Add(text, storage->workforce[tier] >= 1000 ? Tone::Good : Tone::Bad);
        }
        for (int n = 0; n < definition.needCount; n++) {
            const Need& need = definition.needs[n];
            if (need.kind != NeedKind::Good) continue;
            InfoLine& line = m_Lines.Add(NeedName(need));
            line.icon = ItemIcon(need.item);
            std::snprintf(text, sizeof(text), "%d%%", storage->supply[tier][n] / 10);
            line.value = text;
            line.bar = storage->supply[tier][n] / 1000.0f;
        }
    }

    // Storage capacity (the goods are in the inventory window)
    std::snprintf(text, sizeof(text), "Storage: %d per good", storage->CapacityPerItem());
    m_Lines.Add(text, Tone::Heading);
    std::snprintf(text, sizeof(text), "%d warehouse%s", storage->warehouseCount, storage->warehouseCount == 1 ? "" : "s");
    m_Lines.Add(text, Tone::Muted);
    m_Lines.End(m_Model);
}
