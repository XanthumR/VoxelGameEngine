#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
class ElementDocument;
}

// The pause menu Esc opens (assets/ui/pause_menu.rml), as in Anno 1800: Resume, the settings
// (render scale, view distance, UI scale: each a slider with - and + for exact steps; vsync) and
// Quit. The game is paused while it is open. Its clicks are requests the application takes each frame.
class PauseMenu {
public:
    struct Settings {
        int renderScalePercent = 100;
        int viewDistance = 24; // Chunks
        int uiScalePercent = 100;
        bool vsync = true;
    };
    enum Request { NONE, RESUME, QUIT, RENDER_SCALE, VIEW_DISTANCE, UI_SCALE, VSYNC };

    bool Init(Rml::Context* context);
    void SetOpen(bool open);
    bool IsOpen() const { return m_Open; }
    void Update(const Settings& settings); // Shows the current values

    // The request clicked; for a setting, either a step (-1 or +1, absolute false) or the value a
    // slider was moved to (absolute true)
    Request TakeRequest(int& step, bool& absolute, int& value);

private:
    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    bool m_Open = false;
    Settings m_Shown;
    Request m_Request = NONE;
    int m_Step = 0, m_Value = 0;
    bool m_Absolute = false;
};
