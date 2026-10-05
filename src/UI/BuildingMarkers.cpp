#include "UI/BuildingMarkers.h"

#include "Gameplay/Camera.h"
#include "Gameplay/Picking.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/GameObjects.h"
#include "World/WorldConstants.h"

#include "imgui.h"

namespace {

const float MAX_MARKER_DISTANCE = 1.5f; // World units (1920 voxels)
const float BADGE_RADIUS = 11.0f;       // Pixels

} // namespace

void DrawBuildingMarkers(const ICamera& camera, const GameObjectRegistry& objects, glm::ivec2 windowSize) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList(); // Behind the UI windows
    ImU32 red = ImGui::ColorConvertFloat4ToU32(ImVec4(0.85f, 0.12f, 0.10f, 0.95f));
    ImU32 amber = ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.62f, 0.10f, 0.95f));
    ImU32 white = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.6f));

    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
        const LogisticsComponent& logistics = objects.Logistics(id);
        if (type.role == BuildingRole::Storage) continue; // Never needs a road to itself

        // Red: no road to a warehouse. Amber: a house no marketplace serves.
        bool noRoad = !logistics.connected;
        bool noMarket = type.role == BuildingRole::Residence && !logistics.inMarketRange;
        if (!noRoad && !noMarket) continue;

        // Above the middle of the roof
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::vec3 top = glm::vec3(anchor.origin) + glm::vec3(anchor.footprint.x * 0.5f, (float)BuildingHeight(type) + 4.0f, anchor.footprint.y * 0.5f);
        glm::vec3 world = top / VOXELS_PER_UNIT;
        if (glm::length(world - camera.FocusPoint()) > MAX_MARKER_DISTANCE) continue;

        glm::vec2 screen;
        if (!WorldToScreen(camera, world, windowSize, screen)) continue;
        ImVec2 center(screen.x, screen.y);
        drawList->AddCircleFilled(center, BADGE_RADIUS + 1.5f, outline);
        drawList->AddCircleFilled(center, BADGE_RADIUS, noRoad ? red : amber);
        if (noRoad) {
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
