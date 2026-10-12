#pragma once

#include "Simulation/Ships.h"
#include "UI/InfoLines.h"

class ShipControl;
class Simulation;
namespace Rml {
class Context;
class ElementDocument;
}

// The selected ship's panel (assets/ui/ship_panel.rml): where it is, its route and cargo, and while
// docked the goods it can load from or unload to that island, 10 at a time; at a trader's trading
// post the goods it can buy and sell there, with their prices. orderFailed shows that
// the last move order found no way by sea. Clicks are requests the application takes each frame.
class ShipPanel {
public:
    struct GoodRow {
        int item = 0;
        Rml::String name, icon;
        int island = 0, ship = 0;
        bool shown = false, canLoad = false, canUnload = false;
        Rml::String amounts, loadLabel, unloadLabel;
        bool operator==(const GoodRow&) const = default;
    };

    bool Init(Rml::Context* context);
    void Update(ShipId ship, const Simulation& simulation, bool orderFailed);
    // Returns true when goods were bought or sold at a trading post (the coins sound)
    bool ApplyRequests(Simulation& simulation, ShipControl& control);

private:
    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    ShipId m_Ship = INVALID_SHIP;

    // Bound
    InfoLines m_Lines;
    bool m_OnRoute = false, m_Docked = false;
    Rml::String m_Heading;
    std::vector<GoodRow> m_Goods; // Always ITEM_COUNT rows

    int m_LoadRequest = -1, m_UnloadRequest = -1;
    bool m_LeaveRouteRequest = false, m_CloseRequest = false;
};
