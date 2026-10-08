#include "UI/WorldMarkers.h"

#include "Gameplay/Camera.h"
#include "Gameplay/Picking.h"
#include "Economy/PopulationSystem.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/GameObjects.h"
#include "World/WorldConstants.h"

#include "imgui.h"

namespace {

const float MAX_MARKER_DISTANCE = 1.5f; // World units (1920 voxels)
const float BADGE_RADIUS = 11.0f;       // Pixels

// The screen point above the ship, placed between ticks like the ship itself
bool AboveShip(const Ship& ship, float alpha, const ICamera& camera, glm::ivec2 windowSize, glm::vec2& screen) {
    glm::vec2 column = glm::mix(ship.previous, ship.position, glm::clamp(alpha, 0.0f, 1.0f)) * (float)TILE_SIZE;
    return WorldToScreen(camera, glm::vec3(column.x, SEA_LEVEL + 30.0f, column.y) / VOXELS_PER_UNIT, windowSize, screen);
}

// A ring and an arrow over the selected ship
void DrawMarker(const Ship& ship, float alpha, const ICamera& camera, glm::ivec2 windowSize) {
    glm::vec2 screen;
    if (!AboveShip(ship, alpha, camera, windowSize, screen)) return;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.85f, 1.0f, 0.95f));
    ImVec2 at(screen.x, screen.y);
    draw->AddCircle(at, 11.0f, color, 0, 3.0f);
    draw->AddTriangleFilled(ImVec2(at.x - 6.0f, at.y + 14.0f), ImVec2(at.x + 6.0f, at.y + 14.0f), ImVec2(at.x, at.y + 22.0f), color);
}

} // namespace

void DrawBuildingMarkers(const ICamera& camera, const GameObjectRegistry& objects, glm::ivec2 windowSize) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList(); // Behind the UI windows
    ImU32 red = ImGui::ColorConvertFloat4ToU32(ImVec4(0.85f, 0.12f, 0.10f, 0.95f));
    ImU32 amber = ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.62f, 0.10f, 0.95f));
    ImU32 green = ImGui::ColorConvertFloat4ToU32(ImVec4(0.20f, 0.65f, 0.25f, 0.95f));
    ImU32 grey = ImGui::ColorConvertFloat4ToU32(ImVec4(0.45f, 0.45f, 0.48f, 0.95f));
    ImU32 white = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.6f));

    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
        const LogisticsComponent& logistics = objects.Logistics(id);
        if (type.role == BuildingRole::Storage || type.role == BuildingRole::Module) continue; // Never need a road

        // Red: no road to a warehouse. Amber: a house no marketplace serves.
        bool noRoad = !logistics.connected;
        bool noMarket = type.role == BuildingRole::Residence && !logistics.inMarketRange;
        // Producers: grey when nobody can work there (no workers, nothing to work with), orange when an input is missing
        ProducerStatus status = type.role == BuildingRole::Producer ? objects.Production(id).status : ProducerStatus::Working;
        bool idle = status == ProducerStatus::NoWorkforce || status == ProducerStatus::BadLocation;
        bool starved = status == ProducerStatus::MissingInput;
        bool ready = type.role == BuildingRole::Residence && PopulationSystem::IsReadyToUpgrade(objects, id);
        if (!noRoad && !noMarket && !idle && !starved && !ready) continue;

        // Above the middle of the roof
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::vec3 top = glm::vec3(anchor.origin) + glm::vec3(anchor.footprint.x * 0.5f, (float)BuildingHeight(type) + 4.0f, anchor.footprint.y * 0.5f);
        glm::vec3 world = top / VOXELS_PER_UNIT;
        if (glm::length(world - camera.FocusPoint()) > MAX_MARKER_DISTANCE) continue;

        glm::vec2 screen;
        if (!WorldToScreen(camera, world, windowSize, screen)) continue;
        ImVec2 center(screen.x, screen.y);
        drawList->AddCircleFilled(center, BADGE_RADIUS + 1.5f, outline);
        bool problem = noRoad || noMarket || idle || starved;
        ImU32 color = noRoad ? red : idle ? grey : (noMarket || starved) ? amber : green;
        drawList->AddCircleFilled(center, BADGE_RADIUS, color);
        if (!problem) {
            // Ready to upgrade: an arrow pointing up
            drawList->AddTriangleFilled(ImVec2(center.x - 6.0f, center.y), ImVec2(center.x, center.y - 7.0f), ImVec2(center.x + 6.0f, center.y), white);
            drawList->AddRectFilled(ImVec2(center.x - 2.0f, center.y), ImVec2(center.x + 2.0f, center.y + 6.0f), white);
        } else if (noRoad || idle || starved) {
            // "!" drawn with shapes so it does not depend on the font size
            drawList->AddRectFilled(ImVec2(center.x - 1.5f, center.y - 7.0f), ImVec2(center.x + 1.5f, center.y + 2.0f), white);
            drawList->AddRectFilled(ImVec2(center.x - 1.5f, center.y + 4.0f), ImVec2(center.x + 1.5f, center.y + 7.0f), white);
        } else {
            // A little market stall: awning and counter
            drawList->AddTriangleFilled(ImVec2(center.x - 7.0f, center.y - 1.0f), ImVec2(center.x, center.y - 7.0f),
                ImVec2(center.x + 7.0f, center.y - 1.0f), white);
            drawList->AddRectFilled(ImVec2(center.x - 5.0f, center.y + 1.0f), ImVec2(center.x + 5.0f, center.y + 6.0f), white);
        }
    }
}

void DrawShipMarkers(const ShipSystem& ships, ShipId selected, float alpha, const ICamera& camera, glm::ivec2 windowSize) {
    if (ships.IsAlive(selected)) DrawMarker(ships.Get(selected), alpha, camera, windowSize);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImU32 amber = ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.62f, 0.10f, 0.95f));
    ImU32 white = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
    for (int slot = 0; slot < ShipSystem::MAX_SHIPS; slot++) {
        ShipId id = ships.IdAtSlot(slot);
        glm::vec2 screen;
        if (!ships.IsWaitingForRoom(id) || !AboveShip(ships.Get(id), alpha, camera, windowSize, screen)) continue;
        // An amber "!" a little higher than the selection ring
        ImVec2 c(screen.x, screen.y - 30.0f);
        draw->AddCircleFilled(c, 12.5f, outline);
        draw->AddCircleFilled(c, 11.0f, amber);
        draw->AddRectFilled(ImVec2(c.x - 1.5f, c.y - 7.0f), ImVec2(c.x + 1.5f, c.y + 2.0f), white);
        draw->AddRectFilled(ImVec2(c.x - 1.5f, c.y + 4.0f), ImVec2(c.x + 1.5f, c.y + 7.0f), white);
    }
}
