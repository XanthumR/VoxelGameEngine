#pragma once

#include "Simulation/GameObjects.h"

class IslandEconomyManager;
class Simulation;

// Tooltip next to the cursor for the building under it: a house's residents, needs and upgrade
// progress; the houses a marketplace serves; a warehouse's island goods. Nothing for
// INVALID_GAME_OBJECT. Call between ImGui::NewFrame and ImGui::Render.
void DrawBuildingInfo(GameObjectId building, const GameObjectRegistry& objects, const IslandEconomyManager& economy);

// The panel of a clicked building (right side of the screen): the same details, for houses the
// Upgrade button and for harbors the Build ship button. Returns false when the player closed it or
// the building is gone.
bool DrawBuildingPanel(GameObjectId building, Simulation& simulation);
