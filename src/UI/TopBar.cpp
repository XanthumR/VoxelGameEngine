#include "UI/TopBar.h"

#include "Economy/Treasury.h"

#include "imgui.h"

void DrawTopBar(const Treasury& treasury, int& speed) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 8.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::Begin("Top bar", nullptr, flags);

    const ImVec4 red(1.0f, 0.35f, 0.3f, 1.0f), green(0.45f, 0.9f, 0.45f, 1.0f);
    int net = treasury.IncomePerMinute() - treasury.UpkeepPerMinute();
    if (treasury.Coins() < 0) ImGui::TextColored(red, "Coins %lld", (long long)treasury.Coins());
    else ImGui::Text("Coins %lld", (long long)treasury.Coins());
    ImGui::SameLine();
    ImGui::TextColored(net < 0 ? red : green, "%+d / min", net);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Taxes  +%d / min\nUpkeep -%d / min", treasury.IncomePerMinute(), treasury.UpkeepPerMinute());

    // Speed: pause, 1x, 2x, 4x (keys P, + and -)
    const int speeds[4] = { 0, 1, 2, 4 };
    const char* labels[4] = { "||", "1x", "2x", "4x" };
    for (int i = 0; i < 4; i++) {
        ImGui::SameLine();
        bool active = speed == speeds[i];
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.15f, 1.0f));
        if (ImGui::SmallButton(labels[i])) speed = speeds[i];
        if (active) ImGui::PopStyleColor();
    }
    ImGui::End();
}
