#include "UI/TradeRoutes.h"

#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"
#include "UI/InfoLines.h"

#include <RmlUi/Core.h>

#include <cstdio>

namespace {

bool IsHarbor(const GameObjectRegistry& objects, GameObjectId id) {
    if (!objects.IsAlive(id)) return false;
    const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
    return type.role == BuildingRole::Storage && type.dockRows > 0;
}

const char* ActionText(int action) {
    return action == (int)StopAction::Load ? "Load" : action == (int)StopAction::Unload ? "Unload" : "-";
}

// The harbor before or after current in slot order (wrapping around); the first one when current is none
GameObjectId StepHarbor(const GameObjectRegistry& objects, GameObjectId current, int direction) {
    uint32_t count = objects.SlotCount();
    if (count == 0) return current;
    int start = IsHarbor(objects, current) ? (int)(current & 0xFFFF) : (direction > 0 ? -1 : 0);
    for (uint32_t step = 1; step <= count; step++) {
        int slot = ((start + direction * (int)step) % (int)count + (int)count) % (int)count;
        GameObjectId id = objects.IdAtSlot((uint32_t)slot);
        if (id != INVALID_GAME_OBJECT && IsHarbor(objects, id)) return id;
    }
    return current;
}

} // namespace

bool TradeRoutes::Init(Rml::Context* context) {
    Rml::DataModelConstructor model = context->CreateDataModel("trade_routes");
    if (!model) return false;
    if (Rml::StructHandle<GoodToggle> good = model.RegisterStruct<GoodToggle>()) {
        good.RegisterMember("good", &GoodToggle::good);
        good.RegisterMember("action", &GoodToggle::action);
        good.RegisterMember("icon", &GoodToggle::icon);
        good.RegisterMember("label", &GoodToggle::label);
    }
    model.RegisterArray<std::vector<GoodToggle>>();
    if (Rml::StructHandle<StopView> stop = model.RegisterStruct<StopView>()) {
        stop.RegisterMember("index", &StopView::index);
        stop.RegisterMember("used", &StopView::used);
        stop.RegisterMember("harbor", &StopView::harbor);
        stop.RegisterMember("goods", &StopView::goods);
    }
    model.RegisterArray<std::vector<StopView>>();
    if (Rml::StructHandle<RouteView> route = model.RegisterStruct<RouteView>()) {
        route.RegisterMember("index", &RouteView::index);
        route.RegisterMember("used", &RouteView::used);
        route.RegisterMember("can_add", &RouteView::canAdd);
        route.RegisterMember("can_remove", &RouteView::canRemove);
        route.RegisterMember("header", &RouteView::header);
        route.RegisterMember("stops", &RouteView::stops);
    }
    model.RegisterArray<std::vector<RouteView>>();
    model.Bind("routes", &m_Routes);
    model.Bind("can_assign", &m_CanAssign);

    // Events: each leaves one request (route, stop, value from the arguments)
    auto bind = [&](const char* name, Kind kind) {
        model.BindEventCallback(name, [this, kind](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
            m_Request.kind = kind;
            m_Request.route = arguments.size() > 0 ? arguments[0].Get<int>() : 0;
            m_Request.stop = arguments.size() > 1 ? arguments[1].Get<int>() : 0;
            m_Request.value = arguments.size() > 2 ? arguments[2].Get<int>() : 0;
        });
    };
    bind("new_route", Kind::NewRoute);
    bind("delete_route", Kind::DeleteRoute);
    bind("add_stop", Kind::AddStop);
    bind("remove_stop", Kind::RemoveStop);
    bind("assign", Kind::Assign);
    bind("cycle", Kind::Cycle);
    bind("harbor", Kind::Harbor);
    bind("close", Kind::Close);
    m_Model = model.GetModelHandle();

    // Every slot exists from the start; Update only overwrites them
    m_Routes.resize(ShipSystem::MAX_ROUTES);
    for (int r = 0; r < ShipSystem::MAX_ROUTES; r++) {
        m_Routes[r].index = r;
        m_Routes[r].stops.resize(TradeRoute::MAX_STOPS);
        for (int s = 0; s < TradeRoute::MAX_STOPS; s++) {
            m_Routes[r].stops[s].index = s;
            m_Routes[r].stops[s].goods.resize(ITEM_COUNT);
            for (int g = 0; g < ITEM_COUNT; g++) {
                m_Routes[r].stops[s].goods[g].good = g;
                m_Routes[r].stops[s].goods[g].icon = ItemIcon((ItemType)g);
            }
        }
    }
    m_Pending = m_Routes;

    m_Document = context->LoadDocument("assets/ui/trade_routes.rml");
    return m_Document != nullptr;
}

void TradeRoutes::Update(bool open, const Simulation& simulation, ShipId selectedShip) {
    if (m_Document && m_Document->IsVisible() != open) {
        if (open) m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        else m_Document->Hide();
    }
    if (!open) return;

    const ShipSystem& ships = simulation.Ships();
    const GameObjectRegistry& objects = simulation.Objects();
    char text[64];
    for (int r = 0; r < ShipSystem::MAX_ROUTES; r++) {
        const TradeRoute& route = ships.Route(r);
        RouteView& view = m_Pending[r];
        view.used = route.used;
        if (!route.used) continue;
        int shipsOnIt = 0;
        for (int slot = 0; slot < ShipSystem::MAX_SHIPS; slot++) {
            ShipId id = ships.IdAtSlot(slot);
            shipsOnIt += id != INVALID_SHIP && ships.Get(id).route == r ? 1 : 0;
        }
        std::snprintf(text, sizeof(text), "Route %d  (%d stops, %d ships)", r + 1, route.stopCount, shipsOnIt);
        view.header = text;
        view.canAdd = route.stopCount < TradeRoute::MAX_STOPS;
        view.canRemove = route.stopCount > 0;
        for (int s = 0; s < TradeRoute::MAX_STOPS; s++) {
            StopView& stop = view.stops[s];
            stop.used = s < route.stopCount;
            if (!stop.used) continue;
            GameObjectId harbor = route.stops[s].harbor;
            if (IsHarbor(objects, harbor)) {
                std::snprintf(text, sizeof(text), "Harbor %u, island #%u", (harbor & 0xFFFF) + 1, objects.Building(harbor).island);
            } else {
                std::snprintf(text, sizeof(text), "(no harbor)");
            }
            stop.harbor = text;
            for (int g = 0; g < ITEM_COUNT; g++) {
                stop.goods[g].action = (int)route.stops[s].actions[g];
                stop.goods[g].label = ActionText(stop.goods[g].action);
            }
        }
    }
    if (!(m_Pending == m_Routes)) {
        m_Routes = m_Pending; // Same shapes: element-wise copies
        m_Model.DirtyVariable("routes");
    }
    bool canAssign = ships.IsAlive(selectedShip);
    if (canAssign != m_CanAssign) {
        m_CanAssign = canAssign;
        m_Model.DirtyVariable("can_assign");
    }
}

bool TradeRoutes::ApplyRequests(Simulation& simulation, ShipId selectedShip) {
    Request request = m_Request;
    m_Request = Request();
    ShipSystem& ships = simulation.Ships();
    if (request.route < 0 || request.route >= ShipSystem::MAX_ROUTES) return true;
    TradeRoute& route = ships.Route(request.route);
    bool stopValid = request.stop >= 0 && request.stop < route.stopCount;
    switch (request.kind) {
    case Kind::None: break;
    case Kind::NewRoute: ships.CreateRoute(); break;
    case Kind::DeleteRoute: ships.DeleteRoute(request.route); break;
    case Kind::AddStop:
        if (route.used && route.stopCount < TradeRoute::MAX_STOPS) route.stops[route.stopCount++] = RouteStop();
        break;
    case Kind::RemoveStop:
        if (route.used && route.stopCount > 0) route.stopCount--;
        break;
    case Kind::Assign:
        if (route.used && route.stopCount > 0 && ships.IsAlive(selectedShip)) ships.AssignRoute(selectedShip, request.route);
        break;
    case Kind::Cycle:
        if (stopValid && request.value >= 0 && request.value < ITEM_COUNT) {
            StopAction& action = route.stops[request.stop].actions[request.value];
            action = (StopAction)(((int)action + 1) % 3);
        }
        break;
    case Kind::Harbor:
        if (stopValid) route.stops[request.stop].harbor = StepHarbor(simulation.Objects(), route.stops[request.stop].harbor, request.value);
        break;
    case Kind::Close: return false;
    }
    return true;
}
