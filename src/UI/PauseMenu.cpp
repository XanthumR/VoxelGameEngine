#include "UI/PauseMenu.h"

#include <RmlUi/Core.h>

bool PauseMenu::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("pause_menu");
    if (!model) return false;
    model.Bind("render_scale", &m_Shown.renderScalePercent);
    model.Bind("view_distance", &m_Shown.viewDistance);
    model.Bind("ui_scale", &m_Shown.uiScalePercent);
    model.Bind("volume", &m_Shown.volumePercent);
    model.Bind("vsync", &m_Shown.vsync);
    model.Bind("status", &m_Status);
    model.Bind("saves_mode", &m_SavesMode);
    if (Rml::StructHandle<SaveRow> row = model.RegisterStruct<SaveRow>()) {
        row.RegisterMember("name", &SaveRow::name);
        row.RegisterMember("info", &SaveRow::info);
        row.RegisterMember("loadable", &SaveRow::loadable);
    }
    model.RegisterArray<std::vector<SaveRow>>();
    model.Bind("saves", &m_SaveRows);
    // pick(row) / remove(row): a save in the list
    auto row = [this](Request request) {
        return [this, request](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
            if (arguments.empty()) return;
            m_Request = request;
            m_Value = arguments[0].Get<int>();
            m_Absolute = false;
        };
    };
    model.BindEventCallback("pick", row(PICK_SAVE));
    model.BindEventCallback("remove", row(DELETE_SAVE));
    // request(kind, step): kind is a Request
    model.BindEventCallback("request", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (arguments.empty()) return;
        m_Request = (Request)arguments[0].Get<int>();
        m_Step = arguments.size() > 1 ? arguments[1].Get<int>() : 0;
        m_Absolute = false;
    });
    // set(kind, value): a slider moved to value. A slider also reports a change when the data
    // binding sets its value (at load, from the default Settings): only the open menu's count.
    model.BindEventCallback("set", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!m_Open || arguments.size() < 2) return;
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
    set(m_Shown.volumePercent, settings.volumePercent, "volume");
    set(m_Shown.vsync, settings.vsync, "vsync");
}

void PauseMenu::ShowSaves(SavesMode mode, std::vector<SaveRow> rows) {
    if (mode != m_SavesMode) {
        m_SavesMode = mode;
        m_Model.DirtyVariable("saves_mode");
    }
    if (rows != m_SaveRows) {
        m_SaveRows = std::move(rows);
        m_Model.DirtyVariable("saves");
    }
}

void PauseMenu::SetStatus(const Rml::String& status) {
    if (status == m_Status) return;
    m_Status = status;
    m_Model.DirtyVariable("status");
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
