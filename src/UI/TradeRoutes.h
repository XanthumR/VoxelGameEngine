#pragma once

#include "Simulation/Ships.h"

class Simulation;

// The trade route window: create and delete routes, pick each stop's harbor, set what a ship
// loads or unloads there per good (click to cycle: nothing, Load, Unload), and put the selected
// ship on a route. open is cleared when the player closes the window.
// Call between ImGui::NewFrame and ImGui::Render.
void DrawTradeRoutes(bool& open, Simulation& simulation, ShipId selectedShip);
