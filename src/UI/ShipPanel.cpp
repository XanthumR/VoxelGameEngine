#include "UI/ShipPanel.h"

#include "Simulation/Simulation.h"

#include "Gameplay/ShipControl.h"

#include <RmlUi/Core.h>

#include <cstdio>

bool ShipPanel::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("ship_panel");
    if (!model) return false;
    m_Lines.Bind(model, "lines");
    if (Rml::StructHandle<GoodRow> row = model.RegisterStruct<GoodRow>()) {
        row.RegisterMember("item", &GoodRow::item);
        row.RegisterMember("name", &GoodRow::name);
        row.RegisterMember("icon", &GoodRow::icon);
        row.RegisterMember("island", &GoodRow::island);
        row.RegisterMember("ship", &GoodRow::ship);
    }
    model.RegisterArray<std::vector<GoodRow>>();
    model.Bind("goods", &m_Goods);
    model.Bind("on_route", &m_OnRoute);
    model.Bind("docked", &m_Docked);
    auto argument = [](const Rml::VariantList& arguments) { return arguments.empty() ? -1 : arguments[0].Get<int>(); };
    model.BindEventCallback("load", [this, argument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) { m_LoadRequest = argument(a); });
    model.BindEventCallback("unload", [this, argument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) { m_UnloadRequest = argument(a); });
    model.BindEventCallback("leave_route", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_LeaveRouteRequest = true; });
    model.BindEventCallback("close", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_CloseRequest = true; });
    m_Model = model.GetModelHandle();
    m_Goods.resize(ITEM_COUNT);
    for (int i = 0; i < ITEM_COUNT; i++) {
        m_Goods[i].item = i;
        m_Goods[i].name = ITEM_NAMES[i];
        m_Goods[i].icon = ItemIcon((ItemType)i);
    }
    m_Document = context->LoadDocument("assets/ui/ship_panel.rml");
    return m_Document != nullptr;
}

void ShipPanel::Update(ShipId id, const Simulation& simulation, bool orderFailed) {
    const ShipSystem& ships = simulation.Ships();
    bool visible = ships.IsAlive(id);
    m_Ship = visible ? id : INVALID_SHIP;
    if (m_Document && m_Document->IsVisible() != visible) {
        if (visible) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        else m_Document->Hide();
    }
    if (!visible) return;

    char text[96];
    const Ship& ship = ships.Get(id);
    const GameObjectRegistry& objects = simulation.Objects();
    bool atHarbor = objects.IsAlive(ship.harbor);
    IslandId island = atHarbor ? objects.Building(ship.harbor).island : NO_ISLAND;
    m_Lines.Begin();
    switch (ship.state) {
    case ShipState::Idle: std::snprintf(text, sizeof(text), "Anchored at sea"); break;
    case ShipState::Sailing:
        if (atHarbor) std::snprintf(text, sizeof(text), "Sailing to the harbor of island #%u", island);
        else std::snprintf(text, sizeof(text), "Sailing");
        break;
    case ShipState::Docked: std::snprintf(text, sizeof(text), "Docked at the harbor of island #%u", island); break;
    }
    m_Lines.Add(text).icon = "icons/ship.tga";
    if (orderFailed) m_Lines.Add("No way by sea to there", Tone::Warn).icon = "icons/warning.tga";
    if (ship.route >= 0) {
        std::snprintf(text, sizeof(text), "Trade route %d, stop %d of %d", ship.route + 1, ship.stop + 1, ships.Route(ship.route).stopCount);
        m_Lines.Add(text, Tone::Heading);
        if (ships.IsWaitingForRoom(id)) m_Lines.Add("Waiting: the island's storage is full", Tone::Warn).icon = "icons/warning.tga";
    }
    m_Lines.Add("Right click the sea or a harbor to sail there", Tone::Muted);
    for (int i = 0; i < (int)ship.cargo.size(); i++) {
        const CargoSlot& slot = ship.cargo[i];
        std::snprintf(text, sizeof(text), "Hold %d", i + 1);
        InfoLine& line = m_Lines.Add(text, slot.amount == 0 ? Tone::Muted : Tone::Normal);
        if (slot.amount == 0) {
            line.value = "empty";
            continue;
        }
        line.icon = ItemIcon(slot.item);
        std::snprintf(text, sizeof(text), "%d / %d %s", slot.amount, ShipSystem::SLOT_CAPACITY, ItemName(slot.item));
        line.value = text;
        line.bar = (float)slot.amount / ShipSystem::SLOT_CAPACITY;
    }
    m_Lines.End(m_Model);

    auto set = [this](bool& field, bool value, const char* name) {
        if (field == value) return;
        field = value;
        m_Model.DirtyVariable(name);
    };
    set(m_OnRoute, ship.route >= 0, "on_route");
    const IslandStorage* storage = simulation.Economy().Find(island);
    set(m_Docked, ship.state == ShipState::Docked && storage, "docked");
    if (!m_Docked) return;
    bool changed = false;
    for (GoodRow& row : m_Goods) {
        int onShip = ships.CargoAmount(id, (ItemType)row.item), onIsland = storage->Amount((ItemType)row.item);
        changed |= row.ship != onShip || row.island != onIsland;
        row.ship = onShip;
        row.island = onIsland;
    }
    if (changed) m_Model.DirtyVariable("goods");
}

void ShipPanel::ApplyRequests(Simulation& simulation, ShipControl& control) {
    ShipSystem& ships = simulation.Ships();
    if (ships.IsAlive(m_Ship)) {
        if (m_LoadRequest >= 0) ships.Transfer(m_Ship, (ItemType)m_LoadRequest, 10, simulation.Objects(), simulation.Economy());
        if (m_UnloadRequest >= 0) ships.Transfer(m_Ship, (ItemType)m_UnloadRequest, -10, simulation.Objects(), simulation.Economy());
        if (m_LeaveRouteRequest) ships.AssignRoute(m_Ship, -1);
    }
    if (m_CloseRequest) control.Deselect();
    m_LoadRequest = m_UnloadRequest = -1;
    m_LeaveRouteRequest = m_CloseRequest = false;
}
