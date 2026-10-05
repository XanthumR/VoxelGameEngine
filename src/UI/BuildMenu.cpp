#include "UI/BuildMenu.h"

#include "Gameplay/BuildTool.h"
#include "Simulation/BuildingTypes.h"

#include "imgui.h"

#include <cstdio>

namespace {

const ImVec2 BUTTON_SIZE(150.0f, 52.0f);
const ImVec4 SELECTED_COLOR(0.85f, 0.55f, 0.15f, 0.9f);
const ImVec4 ERROR_COLOR(1.0f, 0.45f, 0.4f, 1.0f);
const ImVec4 WARNING_COLOR(1.0f, 0.85f, 0.3f, 1.0f);
const ImVec4 OK_COLOR(0.5f, 1.0f, 0.55f, 1.0f);

// A selectable button; clicking the selected one drops the selection
void SelectButton(BuildTool& tool, int selection, const char* label) {
    bool selected = tool.SelectedType() == selection;
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, SELECTED_COLOR);
    if (ImGui::Button(label, BUTTON_SIZE)) tool.SelectType(selected ? BuildTool::NO_TYPE : selection);
    if (selected) ImGui::PopStyleColor();
    ImGui::SameLine();
}

} // namespace

void DrawBuildMenu(BuildTool& tool, uint32_t buildingCount) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 bottomCenter(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y - 12.0f);
    ImGui::SetNextWindowPos(bottomCenter, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    ImGui::Begin("Build Menu", nullptr, flags);

    // One button per building type, then the road tool
    char label[64];
    for (int type = 0; type < (int)BUILDING_TYPES.size(); type++) {
        const BuildingType& building = BUILDING_TYPES[type];
        std::snprintf(label, sizeof(label), "%s\n%dx%d  [%d]", building.name, building.footprintWidth, building.footprintDepth, type + 1);
        SelectButton(tool, type, label);
    }
    std::snprintf(label, sizeof(label), "Road\n1x1  [%d]", (int)BUILDING_TYPES.size() + 1);
    SelectButton(tool, BuildTool::ROAD, label);

    ImGui::BeginDisabled(tool.SelectedType() == BuildTool::NO_TYPE);
    if (ImGui::Button("Cancel", ImVec2(80.0f, BUTTON_SIZE.y))) tool.SelectType(BuildTool::NO_TYPE);
    ImGui::EndDisabled();

    // Status line
    if (tool.SelectedType() == BuildTool::ROAD) {
        ImGui::TextDisabled("Drag: build road | right-drag: remove road | right click: cancel");
    } else if (tool.SelectedType() != BuildTool::NO_TYPE) {
        PlacementError error = tool.LastError();
        if (!tool.HasPlacementPreview()) {
            ImGui::TextDisabled("Point at an island");
        } else if (error != PlacementError::None) {
            ImGui::TextColored(ERROR_COLOR, "%s", PlacementErrorText(error));
        } else if (tool.SelectedType() != BUILDING_WAREHOUSE && !tool.PreviewConnected()) {
            ImGui::TextColored(WARNING_COLOR, "OK, but no road to a warehouse here");
        } else {
            ImGui::TextColored(OK_COLOR, "OK");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| R rotate, left click place, right click cancel");
    } else {
        ImGui::TextDisabled("Buildings: %u | right click a building or road to demolish it", buildingCount);
    }

    ImGui::End();
}
