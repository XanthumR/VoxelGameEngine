#include "UI/Hud.h"

#include "imgui.h"

#include <cmath>

namespace {

ImU32 HudColor(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

} // namespace

void DrawHud(glm::vec3 crosshairColor, int selectedBlock) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float cx = std::floor(display.x * 0.5f);
    float cy = std::floor(display.y * 0.5f);
    auto rect = [drawList](float x0, float y0, float x1, float y1, ImU32 color) {
        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color);
    };

    // Crosshair: black outline, then the scan-colored lines (center pixel left open)
    ImU32 black = HudColor(0.0f, 0.0f, 0.0f);
    rect(cx - 9, cy - 1, cx, cy + 2, black);
    rect(cx + 1, cy - 1, cx + 10, cy + 2, black);
    rect(cx - 1, cy - 9, cx + 2, cy, black);
    rect(cx - 1, cy + 1, cx + 2, cy + 10, black);
    ImU32 inner = HudColor(crosshairColor.r, crosshairColor.g, crosshairColor.b);
    rect(cx - 8, cy, cx - 1, cy + 1, inner);
    rect(cx + 2, cy, cx + 9, cy + 1, inner);
    rect(cx, cy - 8, cx + 1, cy - 1, inner);
    rect(cx, cy + 2, cx + 1, cy + 9, inner);

    // Hotbar: 5 cells of 40 px, 16 px above the bottom edge
    const int CELLS = 5;
    const float CELL = 40.0f;
    float x0 = cx - 102.0f;
    float y0 = display.y - 56.0f;
    float width = 204.0f, height = 40.0f;

    rect(x0, y0, x0 + width, y0 + height, HudColor(0.15f, 0.15f, 0.15f, 0.85f)); // Panel

    ImU32 grey = HudColor(0.3f, 0.3f, 0.3f);
    rect(x0, y0, x0 + width, y0 + 3, grey);                   // Frame top
    rect(x0, y0 + height - 3, x0 + width, y0 + height, grey); // Frame bottom
    rect(x0 + width - 3, y0, x0 + width, y0 + height, grey);  // Frame right
    for (int i = 0; i < CELLS; i++) {                         // Cell separators
        float cellX = x0 + i * CELL;
        rect(cellX, y0, cellX + 3, y0 + height, grey);
        rect(cellX + 37, y0, cellX + 40, y0 + height, grey);
    }

    const glm::vec3 previews[CELLS] = {
        glm::vec3(0.035f, 0.731f, 0.000f), // Grass
        glm::vec3(0.180f, 0.149f, 0.012f), // Dirt
        glm::vec3(0.5f, 0.5f, 0.5f),       // Stone
        glm::vec3(0.85f, 0.75f, 0.45f),    // Sand
        glm::vec3(0.137f, 0.306f, 0.016f), // Plant
    };
    for (int i = 0; i < CELLS; i++) {
        float cellX = x0 + i * CELL;
        rect(cellX + 8, y0 + 8, cellX + 32, y0 + 32, HudColor(previews[i].r, previews[i].g, previews[i].b));
    }

    // Thick white border around the selected slot
    int active = selectedBlock - 1;
    if (active >= 0 && active < CELLS) {
        ImU32 white = HudColor(1.0f, 1.0f, 1.0f);
        float cellX = x0 + active * CELL;
        rect(cellX + 1, y0 + 1, cellX + 39, y0 + 5, white);
        rect(cellX + 1, y0 + 35, cellX + 39, y0 + 39, white);
        rect(cellX + 1, y0 + 1, cellX + 5, y0 + 39, white);
        rect(cellX + 35, y0 + 1, cellX + 39, y0 + 39, white);
    }
}
