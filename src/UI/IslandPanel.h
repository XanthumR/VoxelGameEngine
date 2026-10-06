#pragma once

#include "Simulation/IslandRegistry.h"

class IslandEconomyManager;

// Top-right window with an island's population (per tier, with how well each need is supplied),
// and its storage (goods, capacity, warehouses). An unsettled island shows a hint to build a
// warehouse. Call between ImGui::NewFrame and ImGui::Render.
void DrawIslandPanel(IslandId island, const IslandEconomyManager& economy);
