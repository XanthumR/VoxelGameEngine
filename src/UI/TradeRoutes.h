#pragma once

#include "Simulation/Ships.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <vector>

class Simulation;
namespace Rml {
class Context;
class ElementDocument;
}

// The trade route window (assets/ui/trade_routes.rml): create and delete routes, pick each stop's
// harbor (arrows step through the harbors), set what a ship loads or unloads there per good (click
// to cycle: nothing, Load, Unload), and put the selected ship on a route. Every route and stop slot
// is always in the bound lists (unused ones hidden), so the lists never shrink. Clicks are requests
// the application takes each frame.
class TradeRoutes {
public:
    struct GoodToggle {
        int good = 0, action = 0; // action: StopAction as int
        Rml::String icon, label;
        bool operator==(const GoodToggle&) const = default;
    };
    struct StopView {
        int index = 0;
        bool used = false;
        Rml::String harbor;
        std::vector<GoodToggle> goods;
        bool operator==(const StopView&) const = default;
    };
    struct RouteView {
        int index = 0;
        bool used = false, canAdd = false, canRemove = false;
        Rml::String header;
        std::vector<StopView> stops;
        bool operator==(const RouteView&) const = default;
    };

    bool Init(Rml::Context* context);
    // open: the window is shown; selectedShip may be assigned to a route
    void Update(bool open, const Simulation& simulation, ShipId selectedShip);
    // Returns false when the player closed the window
    bool ApplyRequests(Simulation& simulation, ShipId selectedShip);

private:
    enum class Kind { None, NewRoute, DeleteRoute, AddStop, RemoveStop, Assign, Cycle, Harbor, Close };
    struct Request {
        Kind kind = Kind::None;
        int route = 0, stop = 0, value = 0;
    };

    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    std::vector<RouteView> m_Routes, m_Pending; // Bound / being filled; always MAX_ROUTES
    bool m_CanAssign = false;
    Request m_Request;
};
