#pragma once

#include "Simulation/IslandRegistry.h"
#include "UI/InfoLines.h"

#include <vector>

class IslandEconomyManager;
namespace Rml {
class Context;
class ElementDocument;
}

// The island under the camera, as in Anno 1800: an island bar at the top left
// (assets/ui/island_bar.rml) with its name, its residents per tier and two buttons. Storage opens
// the inventory window (assets/ui/inventory.rml): every good the island has, with its amount, a
// fill bar against the capacity and the change over the last minute, filtered by kind. Details
// opens the island panel (assets/ui/island_panel.rml): population per tier (jobs filled, how well
// each need is supplied) and the storage capacity. An unsettled island shows how to settle it.
class IslandPanel {
public:
    struct Tier {
        Rml::String icon;
        int residents = 0;
        bool operator==(const Tier&) const = default;
    };
    struct Good {
        Rml::String icon, name, amount, trend, trendIcon;
        int trendTone = 0; // Tone: good when rising, bad when falling
        float fill = 0.0f;
        bool operator==(const Good&) const = default;
    };

    bool Init(Rml::Context* context);
    void Update(IslandId island, const IslandEconomyManager& economy, bool visible);

private:
    void UpdateBar(IslandId island, const IslandEconomyManager& economy);
    void UpdateDetails(IslandId island, const IslandEconomyManager& economy);
    void UpdateInventory(IslandId island, const IslandEconomyManager& economy);

    // Island bar
    Rml::ElementDocument* m_Bar = nullptr;
    Rml::DataModelHandle m_BarModel;
    Rml::String m_Name, m_Note; // m_Note: unsettled, or empty
    bool m_Settled = false;
    std::vector<Tier> m_Tiers, m_TierScratch;
    bool m_DetailsOpen = false, m_InventoryOpen = false;

    // Island panel (details)
    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    Rml::String m_Title; // Bound
    InfoLines m_Lines;   // Bound as "lines"

    // Inventory window
    Rml::ElementDocument* m_Inventory = nullptr;
    Rml::DataModelHandle m_InventoryModel;
    Rml::String m_InventoryTitle, m_Capacity;
    int m_Filter = 0; // 0 all, 1 residents' needs, 2 construction, 3 raw and intermediate
    std::vector<Good> m_Goods, m_GoodScratch;
};
