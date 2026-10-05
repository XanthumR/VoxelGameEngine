#include "UI/BuildMenu.h"

#include "Gameplay/BuildTool.h"
#include "Simulation/BuildingTypes.h"

#include "imgui.h"

#include <cstdio>

namespace {

const ImVec2 BUTTON_SIZE(150.0f, 52.0f);
const ImVec4 SELECTED_COLOR(0.85f, 0.55f, 0.15f, 0.9f);
const ImVec4 ERROR_COLOR(1.0f, 0.45f, 0.4f, 1.0f);
const ImVec4 OK_COLOR(0.5f, 1.0f, 0.55f, 1.0f);

} // namespace

void DrawBuildMenu(BuildTool& tool, uint32_t buildingCount) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 bottomCenter(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y - 12.0f);
    ImGui::SetNextWindowPos(bottomCenter, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    ImGui::Begin("Build Menu", nullptr, flags);

    // One button per building type; the selected one is highlighted, clicking it again drops it
    char label[64];
    for (int type = 0; type < (int)BUILDING_TYPES.size(); type++) {
        const BuildingType& building = BUILDING_TYPES[type];
        bool selected = tool.SelectedType() == type;
        std::snprintf(label, sizeof(label), "%s\n%dx%d  [%d]", building.name, building.footprintWidth, building.footprintDepth, type + 1);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, SELECTED_COLOR);
        if (ImGui::Button(label, BUTTON_SIZE)) tool.SelectType(selected ? BuildTool::NO_TYPE : type);
        if (selected) ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(tool.SelectedType() == BuildTool::NO_TYPE);
    if (ImGui::Button("Cancel", ImVec2(80.0f, BUTTON_SIZE.y))) tool.SelectType(BuildTool::NO_TYPE);
    ImGui::EndDisabled();

    // Status line
    if (tool.SelectedType() != BuildTool::NO_TYPE) {
        PlacementError error = tool.LastError();
        if (tool.Preview().state == BuildPreview::NONE) {
            ImGui::TextDisabled("Point at an island");
        } else {
            ImGui::TextColored(error == PlacementError::None ? OK_COLOR : ERROR_COLOR, "%s", PlacementErrorText(error));
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| R rotate, left click place, right click cancel");
    } else {
        ImGui::TextDisabled("Buildings: %u | right click a building to demolish it", buildingCount);
    }

    ImGui::End();
}
