#include "UI/IslandPanel.h"

#include "Economy/IslandEconomy.h"

#include "imgui.h"

#include <cstdio>

namespace {

const int DEBUG_GOODS = 10;

// A labelled bar from red (0) to green (1000 per mille)
void SupplyBar(const char* name, int perMille) {
    float fraction = perMille / 1000.0f;
    ImVec4 color(1.0f - fraction * 0.7f, 0.3f + fraction * 0.6f, 0.25f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    char overlay[48];
    std::snprintf(overlay, sizeof(overlay), "%s %d%%", name, perMille / 10);
    ImGui::ProgressBar(fraction, ImVec2(200.0f, 0.0f), overlay);
    ImGui::PopStyleColor();
}

} // namespace

void DrawIslandPanel(IslandId island, IslandEconomyManager& economy) {
    if (island == NO_ISLAND) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 topRight(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f, viewport->WorkPos.y + 12.0f);
    ImGui::SetNextWindowPos(topRight, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    ImGui::Begin("Island", nullptr, flags);

    ImGui::Text("Island #%u", island);
    ImGui::Separator();
    IslandStorage* storage = economy.Find(island);
    if (!storage || storage->warehouseCount == 0) {
        ImGui::TextDisabled("Not settled: build a warehouse");
        ImGui::End();
        return;
    }

    // Population, tier by tier, with the supply of each need
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        const PopulationTier& definition = POPULATION_TIERS[tier];
        if (tier > 0 && storage->population[tier] == 0) continue; // Upper tiers once someone lives there
        ImGui::Text("%s: %d", definition.name, storage->population[tier]);
        for (int n = 0; n < definition.needCount; n++) {
            const Need& need = definition.needs[n];
            if (need.kind != NeedKind::Good) continue;
            SupplyBar(need.name, storage->supply[tier][n]);
        }
    }
    ImGui::Separator();

    // Storage
    ImGui::Text("Warehouses: %d | Capacity: %d per good", storage->warehouseCount, storage->CapacityPerItem());
    if (ImGui::BeginTable("Goods", 2, ImGuiTableFlags_SizingFixedFit)) {
        for (int i = 0; i < ITEM_COUNT; i++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(ITEM_NAMES[i]);
            ImGui::TableNextColumn();
            int amount = storage->amounts[i];
            if (amount == 0) ImGui::TextDisabled("%3d", amount);
            else ImGui::Text("%3d", amount);
        }
        ImGui::EndTable();
    }
    if (ImGui::SmallButton("+10 all goods (debug)")) economy.AddAll(island, DEBUG_GOODS);
    ImGui::End();
}
