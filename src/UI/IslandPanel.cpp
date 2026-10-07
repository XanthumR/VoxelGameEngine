#include "UI/IslandPanel.h"

#include "Economy/IslandEconomy.h"

#include "imgui.h"

#include <cstdio>

namespace {


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

void DrawIslandPanel(IslandId island, const IslandEconomyManager& economy) {
    if (island == NO_ISLAND) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 topRight(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f, viewport->WorkPos.y + 12.0f);
    ImGui::SetNextWindowPos(topRight, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    ImGui::Begin("Island", nullptr, flags);

    ImGui::Text("Island #%u", island);
    ImGui::Separator();
    const IslandStorage* storage = economy.Find(island);
    if (!storage || storage->warehouseCount == 0) {
        if (economy.SettledIslandCount() == 0 || storage) ImGui::TextDisabled("Not settled: build a warehouse");
        else ImGui::TextDisabled("Not settled: anchor a ship within 4 tiles of the coast,\nthen build a warehouse or a harbor (its planks come from the ship)");
        ImGui::End();
        return;
    }

    // Population, tier by tier, with the supply of each need
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        const PopulationTier& definition = POPULATION_TIERS[tier];
        if (tier > 0 && storage->population[tier] == 0 && storage->jobs[tier] == 0) continue; // Upper tiers once they matter
        ImGui::Text("%s: %d", definition.name, storage->population[tier]);
        if (storage->jobs[tier] > 0) {
            ImVec4 color = storage->workforce[tier] >= 1000 ? ImVec4(0.5f, 1.0f, 0.55f, 1.0f) : ImVec4(1.0f, 0.45f, 0.4f, 1.0f);
            ImGui::TextColored(color, "  Jobs: %d (%d%% filled)", storage->jobs[tier], storage->workforce[tier] / 10);
        }
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
    ImGui::End();
}
