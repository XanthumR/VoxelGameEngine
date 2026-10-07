#pragma once

#include "Simulation/Ships.h"

#include <glm/glm.hpp>

class ICamera;
class Simulation;

// The selected ship: a marker over it, and a panel with where it is, its cargo and, while docked,
// buttons that move goods between the ship and that island's storage. orderFailed shows that the
// last move order found no way by sea; alpha is GameClock::Alpha, so the marker moves with the ship
// between ticks. Returns false when the player closed the panel.
// Call between ImGui::NewFrame and ImGui::Render.
// An amber "!" over every ship waiting at a stop whose storage is full
void DrawShipMarkers(const ShipSystem& ships, float alpha, const ICamera& camera, glm::ivec2 windowSize);
bool DrawShipPanel(ShipId ship, Simulation& simulation, float alpha, const ICamera& camera, glm::ivec2 windowSize, bool orderFailed);
