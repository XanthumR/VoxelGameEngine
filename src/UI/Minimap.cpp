#include "UI/Minimap.h"

#include "Gameplay/StrategyCamera.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"
#include "World/TerrainGenerator.h"
#include "World/WorldConstants.h"

#include "imgui.h"

#include <RmlUi/Core.h>

#include <cmath>

namespace {

constexpr double REFRESH_SECONDS = 1.0;

uint32_t Pack(int r, int g, int b) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u; // RGBA bytes
}

uint32_t ColorOf(BuildingRole role) {
    switch (role) {
    case BuildingRole::Residence: return Pack(214, 184, 120);
    case BuildingRole::Service: return Pack(236, 200, 96);
    case BuildingRole::Storage: return Pack(240, 232, 214);
    case BuildingRole::Module: return Pack(132, 160, 76);
    case BuildingRole::Trader: return Pack(196, 64, 160); // A trader's
    default: return Pack(184, 98, 64);
    }
}

} // namespace

bool Minimap::Init(Rml::Context* context) {
    m_Base.assign((size_t)MAP_TILES * MAP_TILES, Pack(22, 54, 82));
    m_Pixels = m_Base;
    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MAP_TILES, MAP_TILES, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_Pixels.data());

    Rml::DataModelConstructor model = context->CreateDataModel("minimap");
    if (!model) return false;
    model.BindEventCallback("compass", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_Compass = true; });
    model.BindEventCallback("switch", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        if (m_Session >= 0) m_SwitchTo = (m_Session + 1) % SESSION_COUNT;
    });
    model.Bind("session", &m_SessionName);
    model.Bind("other", &m_OtherName);
    model.Bind("other_open", &m_OtherOpen);
    m_Model = model.GetModelHandle();
    m_Document = context->LoadDocument("assets/ui/minimap.rml");
    return m_Document != nullptr;
}

Minimap::~Minimap() {
    if (m_Texture != 0) glDeleteTextures(1, &m_Texture);
}

void Minimap::ShowSession(int session) {
    m_Session = session;
    m_Origin = SESSIONS[session].originTile;
    m_SessionName = SESSIONS[session].name;
    m_OtherName = SESSIONS[(session + 1) % SESSION_COUNT].name;
    m_Model.DirtyVariable("session");
    m_Model.DirtyVariable("other");
    for (int z = 0; z < MAP_TILES; z++) {
        for (int x = 0; x < MAP_TILES; x++) {
            uint32_t color = Pack(22, 54, 82);
            switch (m_Terrain->TileKindAt(m_Origin.x + x, m_Origin.y + z)) {
            case TerrainGenerator::TileKind::Land: color = Pack(88, 128, 62); break;
            case TerrainGenerator::TileKind::Beach: color = Pack(200, 182, 128); break;
            case TerrainGenerator::TileKind::Cliff: color = Pack(118, 114, 104); break;
            default: break;
            }
            m_Base[(size_t)z * MAP_TILES + x] = color;
        }
    }
    m_LastRefresh = -1.0;
}

void Minimap::SetVisible(bool visible) {
    if (!m_Document || m_Document->IsVisible() == visible) return;
    if (visible) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    else m_Document->Hide();
}

void Minimap::Refresh(const Simulation& simulation) {
    m_Pixels = m_Base; // Same size: no allocation
    auto put = [this](glm::ivec2 tile, uint32_t color) {
        glm::ivec2 p = tile - m_Origin;
        if (p.x >= 0 && p.y >= 0 && p.x < MAP_TILES && p.y < MAP_TILES) m_Pixels[(size_t)p.y * MAP_TILES + p.x] = color;
    };
    simulation.Roads().ForEach([&](glm::ivec2 tile, const RoadTile&) { put(tile, Pack(156, 130, 92)); });
    const GameObjectRegistry& objects = simulation.Objects();
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::ivec2 minTile(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
        glm::ivec2 tiles = anchor.footprint / TILE_SIZE;
        uint32_t color = ColorOf(BUILDING_TYPES[objects.Building(id).type].role);
        for (int z = 0; z < tiles.y; z++) {
            for (int x = 0; x < tiles.x; x++) put(minTile + glm::ivec2(x, z), color);
        }
    }
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MAP_TILES, MAP_TILES, GL_RGBA, GL_UNSIGNED_BYTE, m_Pixels.data());
}

void Minimap::Update(const Simulation& simulation, const StrategyCamera& camera, double now, bool otherOpen) {
    if (!m_Document || !m_Document->IsVisible() || !m_Terrain) return;
    glm::vec3 focusColumn = camera.FocusPoint() * VOXELS_PER_UNIT;
    int session = SessionAtColumn((int)std::floor(focusColumn.x), (int)std::floor(focusColumn.z));
    if (session >= 0 && session != m_Session) ShowSession(session);
    if (m_Session < 0) return;
    if (otherOpen != m_OtherOpen) {
        m_OtherOpen = otherOpen;
        m_Model.DirtyVariable("other_open");
    }
    if (m_LastRefresh < 0.0 || now - m_LastRefresh >= REFRESH_SECONDS) {
        m_LastRefresh = now;
        Refresh(simulation);
    }

    // The map window, in RmlUi's framebuffer pixels, then in ImGui's window coordinates
    Rml::Element* map = m_Document->GetElementById("map");
    if (!map) return;
    Rml::Vector2f offset = map->GetAbsoluteOffset(Rml::BoxArea::Content);
    Rml::Vector2f size = map->GetBox().GetSize(Rml::BoxArea::Content);
    ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    ImVec2 low(offset.x / scale.x, offset.y / scale.y);
    ImVec2 high((offset.x + size.x) / scale.x, (offset.y + size.y) / scale.y);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->AddImage((ImTextureID)(intptr_t)m_Texture, low, high);
    auto toScreen = [&](glm::vec2 tile) {
        glm::vec2 f = (tile - glm::vec2(m_Origin)) / (float)MAP_TILES;
        return ImVec2(low.x + f.x * (high.x - low.x), low.y + f.y * (high.y - low.y));
    };
    draw->PushClipRect(low, high, true);

    // Ships
    const ShipSystem& ships = simulation.Ships();
    for (int slot = 0; slot < ShipSystem::MAX_SHIPS; slot++) {
        ShipId ship = ships.IdAtSlot(slot);
        if (ship == INVALID_SHIP || ships.Get(ship).state == ShipState::Crossing) continue;
        draw->AddCircleFilled(toScreen(ships.Get(ship).position), 3.0f, IM_COL32(255, 255, 255, 255));
        draw->AddCircle(toScreen(ships.Get(ship).position), 3.0f, IM_COL32(20, 20, 20, 255));
    }

    // The camera's view: a box round its focus, turned with it, as big as the zoom shows
    glm::vec3 focus = camera.FocusPoint() * VOXELS_PER_UNIT;
    glm::vec2 center(focus.x / TILE_SIZE, focus.z / TILE_SIZE);
    float yaw = glm::radians(camera.Yaw());
    glm::vec2 forward(std::cos(yaw), std::sin(yaw)), right(-forward.y, forward.x);
    float halfWidth = camera.Distance() * 0.75f / TILE_SIZE, halfDepth = camera.Distance() * 0.5f / TILE_SIZE;
    ImVec2 corners[4] = { toScreen(center + right * halfWidth + forward * halfDepth), toScreen(center - right * halfWidth + forward * halfDepth),
        toScreen(center - right * halfWidth - forward * halfDepth), toScreen(center + right * halfWidth - forward * halfDepth) };
    draw->AddPolyline(corners, 4, IM_COL32(240, 216, 154, 255), ImDrawFlags_Closed, 1.5f);
    draw->PopClipRect();

    // A click inside the window (RmlUi has the mouse there: the game ignores it)
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        if (mouse.x >= low.x && mouse.y >= low.y && mouse.x < high.x && mouse.y < high.y) {
            m_ClickPixel = glm::vec2((mouse.x - low.x) / (high.x - low.x), (mouse.y - low.y) / (high.y - low.y));
        }
    }
}

bool Minimap::TakeClick(glm::vec2& worldColumn) {
    if (m_ClickPixel.x < 0.0f) return false;
    worldColumn = (glm::vec2(m_Origin) + m_ClickPixel * (float)MAP_TILES) * (float)TILE_SIZE;
    m_ClickPixel = glm::vec2(-1.0f);
    return true;
}

bool Minimap::TakeSessionSwitch(int& session) {
    session = m_SwitchTo;
    m_SwitchTo = -1;
    return session >= 0;
}

bool Minimap::TakeCompass() {
    bool compass = m_Compass;
    m_Compass = false;
    return compass;
}
