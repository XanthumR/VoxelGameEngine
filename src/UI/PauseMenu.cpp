#include "UI/PauseMenu.h"

#include <RmlUi/Core.h>

bool PauseMenu::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("pause_menu");
    if (!model) return false;
    model.Bind("render_scale", &m_Shown.renderScalePercent);
    model.Bind("view_distance", &m_Shown.viewDistance);
    model.Bind("ui_scale", &m_Shown.uiScalePercent);
    model.Bind("vsync", &m_Shown.vsync);
    // request(kind, step): kind is a Request
    model.BindEventCallback("request", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (arguments.empty()) return;
        m_Request = (Request)arguments[0].Get<int>();
        m_Step = arguments.size() > 1 ? arguments[1].Get<int>() : 0;
        m_Absolute = false;
    });
    // set(kind, value): a slider moved to value
    model.BindEventCallback("set", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (arguments.size() < 2) return;
        m_Request = (Request)arguments[0].Get<int>();
        m_Value = (int)arguments[1].Get<float>();
        m_Absolute = true;
    });
    m_Model = model.GetModelHandle();
    m_Document = context->LoadDocument("assets/ui/pause_menu.rml");
    return m_Document != nullptr;
}

void PauseMenu::SetOpen(bool open) {
    m_Open = open;
    if (!m_Document || m_Document->IsVisible() == open) return;
    if (open) {
        m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        m_Document->PullToFront();
    } else {
        m_Document->Hide();
    }
}

void PauseMenu::Update(const Settings& settings) {
    auto set = [this](auto& field, const auto& value, const char* name) {
        if (field == value) return;
        field = value;
        m_Model.DirtyVariable(name);
    };
    set(m_Shown.renderScalePercent, settings.renderScalePercent, "render_scale");
    set(m_Shown.viewDistance, settings.viewDistance, "view_distance");
    set(m_Shown.uiScalePercent, settings.uiScalePercent, "ui_scale");
    set(m_Shown.vsync, settings.vsync, "vsync");
}

PauseMenu::Request PauseMenu::TakeRequest(int& step, bool& absolute, int& value) {
    Request request = m_Request;
    step = m_Step;
    absolute = m_Absolute;
    value = m_Value;
    m_Request = NONE;
    m_Step = 0;
    m_Absolute = false;
    return request;
}
