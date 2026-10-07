#include "UI/TopBar.h"

#include "Economy/Treasury.h"

#include <RmlUi/Core.h>

#include <cstdio>

bool TopBar::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("top_bar");
    if (!model) return false;
    model.Bind("coins", &m_Coins);
    model.Bind("net", &m_Net);
    model.Bind("taxes", &m_Taxes);
    model.Bind("upkeep", &m_Upkeep);
    model.Bind("in_debt", &m_InDebt);
    model.Bind("losing", &m_Losing);
    model.Bind("speed", &m_Speed);
    model.Bind("routes_open", &m_RoutesOpen);
    model.BindEventCallback("set_speed", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_SpeedRequest = arguments[0].Get<int>();
    });
    model.BindEventCallback("toggle_routes", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_RoutesToggle = true; });
    m_Model = model.GetModelHandle();

    m_Document = context->LoadDocument("assets/ui/top_bar.rml");
    return m_Document != nullptr;
}

void TopBar::SetVisible(bool visible) {
    if (!m_Document || m_Document->IsVisible() == visible) return;
    if (visible) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    else m_Document->Hide();
}

void TopBar::Update(const Treasury& treasury, int speed, bool routesOpen) {
    char text[32];
    int net = treasury.IncomePerMinute() - treasury.UpkeepPerMinute();
    std::snprintf(text, sizeof(text), "%lld", (long long)treasury.Coins());
    Rml::String coins = text;
    std::snprintf(text, sizeof(text), "%+d / min", net);
    Rml::String netText = text;
    // Only what changed is marked, so the document is laid out again only then
    auto set = [this](auto& field, const auto& value, const char* name) {
        if (field == value) return;
        field = value;
        m_Model.DirtyVariable(name);
    };
    set(m_Coins, coins, "coins");
    set(m_Net, netText, "net");
    set(m_Taxes, Rml::ToString(treasury.IncomePerMinute()), "taxes");
    set(m_Upkeep, Rml::ToString(treasury.UpkeepPerMinute()), "upkeep");
    set(m_InDebt, treasury.Coins() < 0, "in_debt");
    set(m_Losing, net < 0, "losing");
    set(m_Speed, speed, "speed");
    set(m_RoutesOpen, routesOpen, "routes_open");
}

int TopBar::TakeSpeedRequest() {
    int request = m_SpeedRequest;
    m_SpeedRequest = -1;
    return request;
}

bool TopBar::TakeRoutesToggle() {
    bool toggle = m_RoutesToggle;
    m_RoutesToggle = false;
    return toggle;
}
