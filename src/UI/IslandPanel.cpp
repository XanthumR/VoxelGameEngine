#include "UI/IslandPanel.h"

#include "Economy/IslandEconomy.h"

#include "imgui.h"

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
        ImGui::TextDisabled("Not settled: build a warehouse");
    } else {
        ImGui::Text("Warehouses: %d", storage->warehouseCount);
        ImGui::Text("Capacity: %d per good", storage->CapacityPerItem());
        ImGui::Spacing();
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
    }
    ImGui::End();
}
