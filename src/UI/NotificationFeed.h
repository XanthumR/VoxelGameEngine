#pragma once

#include "Simulation/Notifications.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <vector>

class Simulation;
namespace Rml {
class Context;
class ElementDocument;
}

// The notification feed down the right side of the screen, as in Anno 1800
// (assets/ui/notifications.rml): the simulation's notifications as they come, newest on top, at
// most MAX_SHOWN, each fading out after SHOW_SECONDS of real time. Clicking one is a request the
// application takes: the camera glides there and the building opens or the ship is selected.
class NotificationFeed {
public:
    static constexpr int MAX_SHOWN = 6;
    static constexpr double SHOW_SECONDS = 20.0, FADE_SECONDS = 4.0;

    struct Entry {
        Rml::String icon, text, opacity;
        int tone = 0;
        int index = 0; // Into m_Shown
        bool operator==(const Entry&) const = default;
    };

    bool Init(Rml::Context* context);
    void SetVisible(bool visible);
    // now: real time in seconds
    void Update(const Simulation& simulation, double now);
    // The clicked notification; false when none was clicked
    bool TakeClick(Notification& clicked);

private:
    struct Shown {
        Notification notification;
        double since;
    };

    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    std::vector<Shown> m_Shown;               // Oldest first
    std::vector<Entry> m_Entries, m_Scratch;  // Bound: newest first
    uint32_t m_LastSeen = 0;
    int m_ClickRequest = -1;
};
