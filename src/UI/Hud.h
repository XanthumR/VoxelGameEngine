#pragma once

#include <glm/glm.hpp>

// Crosshair and block hotbar, drawn by ImGui at full window resolution so they stay sharp at
// any render scale. Call between ImGui::NewFrame and ImGui::Render.
void DrawHud(glm::vec3 crosshairColor, int selectedBlock);
