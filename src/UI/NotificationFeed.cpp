#include "UI/NotificationFeed.h"

#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"
#include "UI/InfoLines.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

// The line, icon and tone of a notification
void Describe(const Notification& n, char* text, size_t size, const char*& icon, Tone& tone) {
    const char* island = IslandName(n.island);
    const char* building = n.buildingType < BUILDING_TYPES.size() ? BUILDING_TYPES[n.buildingType].name : "building";
    switch (n.kind) {
    case NotificationKind::NoWorkers:
        std::snprintf(text, size, "%s on %s has no workers", building, island);
        icon = "icons/residents.tga";
        tone = Tone::Bad;
        break;
    case NotificationKind::MissingInput:
        std::snprintf(text, size, "%s on %s is waiting for %s", building, island, ItemName((ItemType)n.param));
        icon = ItemIcon((ItemType)n.param);
        tone = Tone::Warn;
        break;
    case NotificationKind::HouseDowngrading:
        std::snprintf(text, size, "Residents are leaving %s %s on %s", std::strchr("AEIOU", building[0]) ? "an" : "a", building, island);
        icon = "icons/warning.tga";
        tone = Tone::Bad;
        break;
    case NotificationKind::UpgradeReady:
        std::snprintf(text, size, "Houses on %s are ready to upgrade", island);
        icon = "icons/upgrade.tga";
        tone = Tone::Good;
        break;
    case NotificationKind::StorageFull:
        std::snprintf(text, size, "%s storage is full on %s", ItemName((ItemType)n.param), island);
        icon = ItemIcon((ItemType)n.param);
        tone = Tone::Warn;
        break;
    case NotificationKind::TierReached:
        std::snprintf(text, size, "The first %s moved in on %s!", POPULATION_TIERS[n.param].name, island);
        icon = TierIcon(n.param);
        tone = Tone::Good;
        break;
    case NotificationKind::OutOfCoins:
        std::snprintf(text, size, "The treasury is empty: no more building until it fills");
        icon = "icons/coin.tga";
        tone = Tone::Bad;
        break;
    case NotificationKind::ShipDocked:
        std::snprintf(text, size, "A ship docked at the harbor of %s", island);
        icon = "icons/ship.tga";
        tone = Tone::Normal;
        break;
    }
}

} // namespace

bool NotificationFeed::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("notifications");
    if (!model) return false;
    if (Rml::StructHandle<Entry> entry = model.RegisterStruct<Entry>()) {
        entry.RegisterMember("icon", &Entry::icon);
        entry.RegisterMember("text", &Entry::text);
        entry.RegisterMember("opacity", &Entry::opacity);
        entry.RegisterMember("tone", &Entry::tone);
        entry.RegisterMember("index", &Entry::index);
    }
    model.RegisterArray<std::vector<Entry>>();
    model.Bind("entries", &m_Entries);
    model.BindEventCallback("open", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_ClickRequest = arguments[0].Get<int>();
    });
    m_Model = model.GetModelHandle();
    m_Shown.reserve(MAX_SHOWN + 1);
    m_Entries.reserve(MAX_SHOWN);
    m_Scratch.reserve(MAX_SHOWN);
    m_Document = context->LoadDocument("assets/ui/notifications.rml");
    return m_Document != nullptr;
}

void NotificationFeed::SetVisible(bool visible) {
    if (!m_Document || m_Document->IsVisible() == visible) return;
    if (visible) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    else m_Document->Hide();
}

void NotificationFeed::Update(const Simulation& simulation, double now) {
    // New ones in; too many or too old out
    simulation.Notifications().ForEachSince(m_LastSeen, [&](const Notification& notification) {
        if (m_Shown.size() == (size_t)MAX_SHOWN) m_Shown.erase(m_Shown.begin());
        m_Shown.push_back({ notification, now }); // Within the reserve
    });
    m_LastSeen = simulation.Notifications().LastSequence();
    m_Shown.erase(std::remove_if(m_Shown.begin(), m_Shown.end(), [now](const Shown& shown) { return now - shown.since > SHOW_SECONDS; }), m_Shown.end());

    m_Scratch.clear();
    char text[128];
    for (int i = (int)m_Shown.size() - 1; i >= 0; i--) {
        const char* icon = "";
        Tone tone = Tone::Normal;
        Describe(m_Shown[i].notification, text, sizeof(text), icon, tone);
        double left = SHOW_SECONDS - (now - m_Shown[i].since);
        float opacity = (float)std::clamp(left / FADE_SECONDS, 0.0, 1.0);
        char fade[16];
        std::snprintf(fade, sizeof(fade), "%.1f", opacity);
        m_Scratch.push_back({ icon, text, fade, (int)tone, i }); // Within the reserve
    }
    if (m_Entries != m_Scratch) {
        m_Entries = m_Scratch;
        m_Model.DirtyVariable("entries");
    }
}

bool NotificationFeed::TakeClick(Notification& clicked) {
    int index = m_ClickRequest;
    m_ClickRequest = -1;
    if (index < 0 || index >= (int)m_Shown.size()) return false;
    clicked = m_Shown[index].notification;
    m_Shown.erase(m_Shown.begin() + index); // Read: it goes
    return true;
}
