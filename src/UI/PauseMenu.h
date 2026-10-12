#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <vector>

namespace Rml {
class Context;
class ElementDocument;
}

// The pause menu Esc opens (assets/ui/pause_menu.rml), as in Anno 1800: Resume, Save game and Load
// game, the settings (render scale, view distance, UI scale: each a slider with - and + for exact
// steps; vsync) and Quit. Save and Load show the list of saves in its place (ShowSaves): a new save
// or one to overwrite, or one to load; each can be deleted. The game is paused while it is open.
// Its clicks are requests the application takes each frame.
class PauseMenu {
public:
    struct Settings {
        int renderScalePercent = 100;
        int viewDistance = 24; // Chunks
        int uiScalePercent = 100;
        int volumePercent = 80;
        bool vsync = true;
    };
    enum Request { NONE, RESUME, QUIT, RENDER_SCALE, VIEW_DISTANCE, UI_SCALE, VSYNC, SAVE, LOAD, NEW_SAVE, PICK_SAVE, DELETE_SAVE, BACK, VOLUME };
    enum SavesMode { NO_SAVES, SAVING, LOADING }; // What the list of saves is shown for

    struct SaveRow {
        Rml::String name, info;
        bool loadable = false;
        bool operator==(const SaveRow&) const = default;
    };

    bool Init(Rml::Context* context);
    void SetOpen(bool open);
    bool IsOpen() const { return m_Open; }
    void Update(const Settings& settings); // Shows the current values
    void SetStatus(const Rml::String& status); // Under the Save and Load buttons
    void ShowSaves(SavesMode mode, std::vector<SaveRow> rows); // NO_SAVES: the main menu again
    SavesMode Saves() const { return (SavesMode)m_SavesMode; }

    // The request clicked; for a setting, either a step (-1 or +1, absolute false) or the value a
    // slider was moved to (absolute true); for PICK_SAVE and DELETE_SAVE the row in value
    Request TakeRequest(int& step, bool& absolute, int& value);

private:
    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    bool m_Open = false;
    Settings m_Shown;
    Rml::String m_Status;
    int m_SavesMode = NO_SAVES;
    std::vector<SaveRow> m_SaveRows;
    Request m_Request = NONE;
    int m_Step = 0, m_Value = 0;
    bool m_Absolute = false;
};
