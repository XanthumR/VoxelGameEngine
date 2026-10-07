#include "UI/TradeRoutes.h"

#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"

#include "imgui.h"

#include <cstdio>

namespace {

bool IsHarbor(const GameObjectRegistry& objects, GameObjectId id) {
    if (!objects.IsAlive(id)) return false;
    const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
    return type.role == BuildingRole::Storage && type.dockRows > 0;
}

void HarborName(const GameObjectRegistry& objects, GameObjectId id, char* out, size_t size) {
    if (!IsHarbor(objects, id)) std::snprintf(out, size, "(no harbor)");
    else std::snprintf(out, size, "Harbor %u on island #%u", (id & 0xFFFF) + 1, objects.Building(id).island);
}

const char* ActionText(StopAction action) {
    return action == StopAction::Load ? "Load" : action == StopAction::Unload ? "Unload" : "-";
}

// One stop: its harbor and what happens to each good there
void DrawStop(RouteStop& stop, int index, const GameObjectRegistry& objects) {
    char name[64];
    HarborName(objects, stop.harbor, name, sizeof(name));
    ImGui::Text("Stop %d", index + 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##harbor", name)) {
        for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
            GameObjectId id = objects.IdAtSlot(slot);
            if (id == INVALID_GAME_OBJECT || !IsHarbor(objects, id)) continue;
            HarborName(objects, id, name, sizeof(name));
            if (ImGui::Selectable(name, id == stop.harbor)) stop.harbor = id;
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginTable("##goods", 4, ImGuiTableFlags_SizingFixedFit)) {
        for (int i = 0; i < ITEM_COUNT; i++) {
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            StopAction& action = stop.actions[i];
            bool active = action != StopAction::None;
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, action == StopAction::Load ? ImVec4(0.2f, 0.55f, 0.25f, 1.0f) : ImVec4(0.65f, 0.4f, 0.15f, 1.0f));
            char label[48];
            std::snprintf(label, sizeof(label), "%s: %s", ITEM_NAMES[i], ActionText(action));
            if (ImGui::Button(label, ImVec2(150.0f, 0.0f))) action = (StopAction)(((int)action + 1) % 3);
            if (active) ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

} // namespace

void DrawTradeRoutes(bool& open, Simulation& simulation, ShipId selectedShip) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.0f, 0.5f));
    if (!ImGui::Begin("Trade routes", &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    ShipSystem& ships = simulation.Ships();
    const GameObjectRegistry& objects = simulation.Objects();
    ImGui::TextDisabled("Ships on a route sail its stops in a loop. Click a good to cycle: -, Load, Unload.");
    if (ImGui::Button("New route")) ships.CreateRoute();

    for (int route = 0; route < ShipSystem::MAX_ROUTES; route++) {
        TradeRoute& r = ships.Route(route);
        if (!r.used) continue;
        int shipsOnIt = 0;
        for (int slot = 0; slot < ShipSystem::MAX_SHIPS; slot++) {
            ShipId id = ships.IdAtSlot(slot);
            shipsOnIt += id != INVALID_SHIP && ships.Get(id).route == route ? 1 : 0;
        }
        char header[64];
        std::snprintf(header, sizeof(header), "Route %d  (%d stops, %d ships)###route%d", route + 1, r.stopCount, shipsOnIt, route);
        ImGui::PushID(route);
        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (int stop = 0; stop < r.stopCount; stop++) {
                ImGui::PushID(stop);
                DrawStop(r.stops[stop], stop, objects);
                ImGui::PopID();
            }
            ImGui::BeginDisabled(r.stopCount >= TradeRoute::MAX_STOPS);
            if (ImGui::SmallButton("Add stop")) r.stops[r.stopCount++] = RouteStop();
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(r.stopCount == 0);
            if (ImGui::SmallButton("Remove last stop")) r.stopCount--;
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!ships.IsAlive(selectedShip) || r.stopCount == 0);
            if (ImGui::SmallButton("Assign selected ship")) ships.AssignRoute(selectedShip, route);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete route")) ships.DeleteRoute(route);
        }
        ImGui::PopID();
    }
    ImGui::End();
}
