#pragma once

#include <cstdint>

class BuildTool;

// The build bar at the bottom of the screen (strategy camera): one button per building type
// (with its hotkey), a cancel button, and a status line with the controls and why the current
// spot is invalid. Call between ImGui::NewFrame and ImGui::Render.
void DrawBuildMenu(BuildTool& tool, uint32_t buildingCount);
