#include "UI/IslandPanel.h"

#include "Economy/IslandEconomy.h"

#include <RmlUi/Core.h>

#include <cstdio>

bool IslandPanel::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("island_panel");
    if (!model) return false;
    model.Bind("title", &m_Title);
    m_Lines.Bind(model, "lines");
    m_Model = model.GetModelHandle();
    m_Document = context->LoadDocument("assets/ui/island_panel.rml");
    return m_Document != nullptr;
}

void IslandPanel::Update(IslandId island, const IslandEconomyManager& economy, bool visible) {
    visible = visible && island != NO_ISLAND;
    if (m_Document && m_Document->IsVisible() != visible) {
        if (visible) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        else m_Document->Hide();
    }
    if (!visible) return;

    char text[96];
    std::snprintf(text, sizeof(text), "Island #%u", island);
    if (m_Title != text) {
        m_Title = text;
        m_Model.DirtyVariable("title");
    }

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
        heading.icon = tier == 0 ? "icons/farmer.tga" : "icons/worker.tga";
        heading.value = Rml::ToString(storage->population[tier]);
        if (storage->jobs[tier] > 0) {
            std::snprintf(text, sizeof(text), "Jobs %d, %d%% filled", storage->jobs[tier], storage->workforce[tier] / 10);
            m_Lines.Add(text, storage->workforce[tier] >= 1000 ? Tone::Good : Tone::Bad);
        }
        for (int n = 0; n < definition.needCount; n++) {
            const Need& need = definition.needs[n];
            if (need.kind != NeedKind::Good) continue;
            InfoLine& line = m_Lines.Add(need.name);
            line.icon = ItemIcon(need.item);
            std::snprintf(text, sizeof(text), "%d%%", storage->supply[tier][n] / 10);
            line.value = text;
            line.bar = storage->supply[tier][n] / 1000.0f;
        }
    }

    // Storage
    std::snprintf(text, sizeof(text), "Storage (%d per good)", storage->CapacityPerItem());
    m_Lines.Add(text, Tone::Heading);
    std::snprintf(text, sizeof(text), "%d warehouse%s", storage->warehouseCount, storage->warehouseCount == 1 ? "" : "s");
    m_Lines.Add(text, Tone::Muted);
    for (int i = 0; i < ITEM_COUNT; i++) {
        InfoLine& line = m_Lines.Add(ITEM_NAMES[i], storage->amounts[i] == 0 ? Tone::Muted : Tone::Normal);
        line.icon = ItemIcon((ItemType)i);
        line.value = Rml::ToString(storage->amounts[i]);
    }
    m_Lines.End(m_Model);
}
