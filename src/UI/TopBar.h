#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "Simulation/GameObjects.h"

#include <vector>

class IslandEconomyManager;
class Treasury;
namespace Rml {
class Context;
class ElementDocument;
}

// Top-center bar (assets/ui/top_bar.rml), as in Anno 1800: coins and the net income per minute
// (red when negative; taxes and upkeep on hover), the residents of each tier reached on all
// islands (hovered: how well each of the tier's needs is met, over all its houses), the game speed
// buttons and the trade routes button. Its clicks are
// requests the application takes each frame, so the speed keys and the buttons never fight.
class TopBar {
public:
    bool Init(Rml::Context* context);
    void SetVisible(bool visible);
    // speed: 0 = paused, 1, 2, 4
    void Update(const Treasury& treasury, const IslandEconomyManager& economy, const GameObjectRegistry& objects, int speed, bool routesOpen);

    struct NeedRow {
        Rml::String icon, name, supply;
        int tone = 0;
        bool operator==(const NeedRow&) const = default;
    };
    struct Tier {
        Rml::String icon, name;
        int residents = 0;
        bool shown = false; // Once the tier has residents (Farmers always)
        std::vector<NeedRow> needs;
    };

    int TakeSpeedRequest();   // -1 when the speed was not clicked
    bool TakeRoutesToggle();  // The routes button was clicked

private:
    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;

    // Bound to the document
    Rml::String m_Coins, m_Net, m_Taxes, m_Upkeep;
    bool m_InDebt = false, m_Losing = false, m_RoutesOpen = false;
    int m_Speed = 1;
    std::vector<Tier> m_Tiers; // One per population tier, bound once

    int m_SpeedRequest = -1;
    bool m_RoutesToggle = false;
};
