#include "UI/ShipPanel.h"

#include "Gameplay/Picking.h"
#include "Simulation/Simulation.h"
#include "World/WorldConstants.h"

#include "imgui.h"

#include <cstdio>

namespace {

const ImVec4 WARNING_COLOR(1.0f, 0.55f, 0.2f, 1.0f);

// A ring and an arrow over the ship, placed between ticks like the ship itself
void DrawMarker(const Ship& ship, float alpha, const ICamera& camera, glm::ivec2 windowSize) {
    glm::vec2 column = glm::mix(ship.previous, ship.position, glm::clamp(alpha, 0.0f, 1.0f)) * (float)TILE_SIZE;
    glm::vec3 top = glm::vec3(column.x, SEA_LEVEL + 30.0f, column.y) / VOXELS_PER_UNIT;
    glm::vec2 screen;
    if (!WorldToScreen(camera, top, windowSize, screen)) return;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.85f, 1.0f, 0.95f));
    ImVec2 at(screen.x, screen.y);
    draw->AddCircle(at, 11.0f, color, 0, 3.0f);
    draw->AddTriangleFilled(ImVec2(at.x - 6.0f, at.y + 14.0f), ImVec2(at.x + 6.0f, at.y + 14.0f), ImVec2(at.x, at.y + 22.0f), color);
}

} // namespace

bool DrawShipPanel(ShipId id, Simulation& simulation, float alpha, const ICamera& camera, glm::ivec2 windowSize, bool orderFailed) {
    ShipSystem& ships = simulation.Ships();
    if (!ships.IsAlive(id)) return false;
    const Ship& ship = ships.Get(id);
    DrawMarker(ship, alpha, camera, windowSize);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f), ImGuiCond_Always,
        ImVec2(1.0f, 0.5f));
    bool open = true;
    ImGui::Begin("Ship", &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

    const GameObjectRegistry& objects = simulation.Objects();
    bool atHarbor = objects.IsAlive(ship.harbor);
    IslandId island = atHarbor ? objects.Building(ship.harbor).island : NO_ISLAND;
    switch (ship.state) {
    case ShipState::Idle: ImGui::TextUnformatted("Anchored at sea"); break;
    case ShipState::Sailing: atHarbor ? ImGui::Text("Sailing to the harbor of island #%u", island) : ImGui::TextUnformatted("Sailing"); break;
    case ShipState::Docked: ImGui::Text("Docked at the harbor of island #%u", island); break;
    }
    if (orderFailed) ImGui::TextColored(WARNING_COLOR, "No way by sea to there");
    ImGui::TextDisabled("Right click the sea or a harbor to sail there");

    ImGui::Separator();
    for (int i = 0; i < (int)ship.cargo.size(); i++) {
        const CargoSlot& slot = ship.cargo[i];
        if (slot.amount == 0) ImGui::TextDisabled("Hold %d: empty", i + 1);
        else ImGui::Text("Hold %d: %d / %d %s", i + 1, slot.amount, ShipSystem::SLOT_CAPACITY, ItemName(slot.item));
    }

    // Docked: goods between the ship and the island
    const IslandStorage* storage = simulation.Economy().Find(island);
    if (ship.state == ShipState::Docked && storage) {
        ImGui::Separator();
        ImGui::TextUnformatted("Load and unload (10 at a time)");
        if (ImGui::BeginTable("Cargo", 4, ImGuiTableFlags_SizingFixedFit)) {
            for (int i = 0; i < ITEM_COUNT; i++) {
                ItemType item = (ItemType)i;
                int onShip = 0;
                for (const CargoSlot& slot : ship.cargo) onShip += slot.item == item ? slot.amount : 0;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ITEM_NAMES[i]);
                ImGui::TableNextColumn();
                ImGui::Text("island %3d  ship %2d", storage->Amount(item), onShip);
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ImGui::BeginDisabled(storage->Amount(item) == 0);
                if (ImGui::SmallButton("Load")) ships.Transfer(id, item, 10, objects, simulation.Economy());
                ImGui::EndDisabled();
                ImGui::TableNextColumn();
                ImGui::BeginDisabled(onShip == 0);
                if (ImGui::SmallButton("Unload")) ships.Transfer(id, item, -10, objects, simulation.Economy());
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
    return open;
}
