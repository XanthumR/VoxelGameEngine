#pragma once

#include "Simulation/ItemType.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <vector>

namespace Rml {
class DataModelConstructor;
}

// Tones a line's text can have (theme.rcss: normal, good, warn, bad, muted, a heading)
enum class Tone : int { Normal, Good, Warn, Bad, Muted, Heading };

// One row of an info panel: text, a value on the right, an icon in front, a bar under it
struct InfoLine {
    Rml::String text, value;
    Rml::String icon; // Path relative to assets/ui, or empty
    int tone = 0;
    float bar = -1.0f; // 0..1 shows a bar, below 0 none

    bool operator==(const InfoLine&) const = default;
};

// The rows of a panel, filled anew each frame and bound to a document as an array: in the document,
// data-for="line : <name>" with line.text, line.value, line.icon, line.tone and line.bar. Rows are
// written over the previous frame's, so the model is only marked dirty when something changed.
class InfoLines {
public:
    // Registers InfoLine (once per data model) and binds this list as name
    void Bind(Rml::DataModelConstructor& model, const char* name);

    void Begin();
    InfoLine& Add(const char* text, Tone tone = Tone::Normal);
    void End(Rml::DataModelHandle model); // Marks the list dirty when the rows differ from last time

private:
    std::vector<InfoLine> m_Lines;   // Bound
    std::vector<InfoLine> m_Pending; // Being filled
    size_t m_Count = 0;
    const char* m_Name = "";
};

// The icon of a good, e.g. "icons/fish.tga"
const char* ItemIcon(ItemType item);
