#pragma once

#include "Simulation/IslandRegistry.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <vector>

class Simulation;
namespace Rml {
class Context;
class ElementDocument;
}

// The statistics window (F2 or the top bar's button; assets/ui/statistics.rml), as in Anno 1800,
// for the island at the camera or all islands together:
//   Production: per good, its producers, made and used per minute over the last minute, the
//               balance and the stock
//   Population: per tier, residents, houses, jobs and how many are filled, and how well each of
//               its needs is met
//   Finance:    taxes per tier and upkeep per building type, per minute, and the balance
class Statistics {
public:
    struct Row {
        Rml::String icon, name, a, b, c, d, e; // Up to five value columns
        int tone = 0;         // Of the last column (Tone)
        bool heading = false; // A group's first row, bold
        bool operator==(const Row&) const = default;
    };

    bool Init(Rml::Context* context);
    void Toggle() { m_Open = !m_Open; }
    bool IsOpen() const { return m_Open; }
    void Close() { m_Open = false; }
    void Update(const Simulation& simulation, IslandId island, bool visible);

private:
    void SetColumns(const char* const (&columns)[5]);
    void FillProduction(const Simulation& simulation, IslandId island);
    void FillPopulation(const Simulation& simulation, IslandId island);
    void FillFinance(const Simulation& simulation, IslandId island);

    Rml::ElementDocument* m_Document = nullptr;
    Rml::DataModelHandle m_Model;
    bool m_Open = false;

    // Bound
    int m_Tab = 0;
    bool m_AllIslands = false;
    Rml::String m_Title, m_Columns[5], m_Summary;
    std::vector<Row> m_Rows, m_Scratch;
};
