#pragma once

#include "Simulation/GameObjects.h"

class IslandEconomyManager;

// Tooltip next to the cursor for the building under it: a house's residents, needs and upgrade
// progress; the houses a marketplace serves; a warehouse's island goods. Nothing for
// INVALID_GAME_OBJECT. Call between ImGui::NewFrame and ImGui::Render.
void DrawBuildingInfo(GameObjectId building, const GameObjectRegistry& objects, const IslandEconomyManager& economy);
