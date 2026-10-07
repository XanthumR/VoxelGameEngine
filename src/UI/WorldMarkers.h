#pragma once

#include "Simulation/Ships.h"

#include <glm/glm.hpp>

class GameObjectRegistry;
class ICamera;

// Badges drawn over the world (ImGui background draw list, strategy camera); the rest of the game
// UI is RmlUi. Call between ImGui::NewFrame and ImGui::Render.

// Problem badges above buildings: no road to a warehouse, no marketplace, no workers, missing input;
// ready-to-upgrade arrows
void DrawBuildingMarkers(const ICamera& camera, const GameObjectRegistry& objects, glm::ivec2 windowSize);

// A ring over the selected ship (INVALID for none) and an amber "!" over every ship waiting at a stop
// whose storage is full. alpha is GameClock::Alpha, so the markers move with the ships between ticks.
void DrawShipMarkers(const ShipSystem& ships, ShipId selected, float alpha, const ICamera& camera, glm::ivec2 windowSize);
