#pragma once

class Treasury;

// Top-center bar: coins and the net income per minute (red when negative; taxes and upkeep on
// hover), the game speed buttons and the trade routes button. speed: 0 = paused, 1, 2, 4; the
// buttons change it; routesOpen toggles the trade route window.
// Call between ImGui::NewFrame and ImGui::Render.
void DrawTopBar(const Treasury& treasury, int& speed, bool& routesOpen);
