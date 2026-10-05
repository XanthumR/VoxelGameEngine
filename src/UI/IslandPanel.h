#pragma once

#include "Simulation/IslandRegistry.h"

class IslandEconomyManager;

// Top-right window with an island's storage: goods, capacity and warehouse count, or a hint to
// build a warehouse on an unsettled island. Call between ImGui::NewFrame and ImGui::Render.
void DrawIslandPanel(IslandId island, const IslandEconomyManager& economy);
