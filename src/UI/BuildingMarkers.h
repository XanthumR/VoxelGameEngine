#pragma once

#include <glm/glm.hpp>

class GameObjectRegistry;
class ICamera;

// Red "!" badges above buildings that have no road connection to a warehouse (strategy camera).
// Call between ImGui::NewFrame and ImGui::Render.
void DrawBuildingMarkers(const ICamera& camera, const GameObjectRegistry& objects, glm::ivec2 windowSize);
