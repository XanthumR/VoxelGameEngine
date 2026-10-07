#include "UI/InfoLines.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <array>

namespace {

constexpr size_t MAX_LINES = 48;

constexpr std::array<const char*, ITEM_COUNT> ITEM_ICONS = { "icons/wood.tga", "icons/planks.tga", "icons/fish.tga", "icons/wool.tga",
    "icons/work_clothes.tga", "icons/bricks.tga", "icons/sausages.tga", "icons/pigs.tga" };

} // namespace

const char* ItemIcon(ItemType item) {
    return ITEM_ICONS[(size_t)item];
}

void InfoLines::Bind(Rml::DataModelConstructor& model, const char* name) {
    // Data types belong to the context, not the model: registered by the first panel only
    static bool registered = false;
    if (!registered) {
        registered = true;
        Rml::StructHandle<InfoLine> line = model.RegisterStruct<InfoLine>();
        line.RegisterMember("text", &InfoLine::text);
        line.RegisterMember("value", &InfoLine::value);
        line.RegisterMember("icon", &InfoLine::icon);
        line.RegisterMember("tone", &InfoLine::tone);
        line.RegisterMember("bar", &InfoLine::bar);
        model.RegisterArray<std::vector<InfoLine>>();
    }
    model.Bind(name, &m_Lines);
    m_Name = name;
    m_Lines.reserve(MAX_LINES);
    m_Pending.resize(MAX_LINES);
}

void InfoLines::Begin() {
    m_Count = 0;
}

InfoLine& InfoLines::Add(const char* text, Tone tone) {
    if (m_Count == m_Pending.size()) m_Count--; // Full: the last row is overwritten
    InfoLine& line = m_Pending[m_Count++];
    line.text = text;
    line.value.clear();
    line.icon.clear();
    line.tone = (int)tone;
    line.bar = -1.0f;
    return line;
}

void InfoLines::End(Rml::DataModelHandle model) {
    bool same = m_Lines.size() == m_Count;
    for (size_t i = 0; same && i < m_Count; i++) same = m_Lines[i] == m_Pending[i];
    if (same) return;
    m_Lines.resize(m_Count); // Within the reserved capacity
    for (size_t i = 0; i < m_Count; i++) m_Lines[i] = m_Pending[i];
    model.DirtyVariable(m_Name);
}
