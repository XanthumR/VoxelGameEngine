#pragma once

#include "Simulation/IslandRegistry.h"
#include "UI/InfoLines.h"

class IslandEconomyManager;
namespace Rml {
class Context;
class ElementDocument;
}

// Top-right panel (assets/ui/island_panel.rml) with an island's population per tier (jobs filled,
// how well each need is supplied) and its storage (goods, capacity, warehouses). An unsettled
// island shows how to settle it. Hidden for NO_ISLAND.
class IslandPanel {
public:
    bool Init(Rml::Context* context);
    void Update(IslandId island, const IslandEconomyManager& economy, bool visible);

private:
    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    Rml::String m_Title; // Bound
    InfoLines m_Lines;   // Bound as "lines"
};
