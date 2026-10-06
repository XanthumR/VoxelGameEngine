#pragma once

class Treasury;

// Top-center bar: coins and the net income per minute (red when negative; taxes and upkeep on
// hover), and the game speed buttons. speed: 0 = paused, 1, 2, 4; the buttons change it.
// Call between ImGui::NewFrame and ImGui::Render.
void DrawTopBar(const Treasury& treasury, int& speed);
