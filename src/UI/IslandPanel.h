#pragma once

#include "Simulation/IslandRegistry.h"

class IslandEconomyManager;

// Top-right window with an island's population (per tier, with how well each need is supplied),
// its storage (goods, capacity, warehouses), and a debug button that adds goods. An unsettled
// island shows a hint to build a warehouse. Call between ImGui::NewFrame and ImGui::Render.
void DrawIslandPanel(IslandId island, IslandEconomyManager& economy);
