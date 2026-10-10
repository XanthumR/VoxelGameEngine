#include "UI/BuildingInfo.h"

#include "Economy/IslandEconomy.h"
#include "Economy/PopulationSystem.h"
#include "Economy/ProductionChains.h"
#include "Economy/Treasury.h"
#include "Gameplay/BuildTool.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/ProducerLocation.h"
#include "Simulation/Simulation.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cstdio>

namespace {

// "2 Planks, 3 Bricks"; false (and empty) when the cost takes no materials
bool MaterialsText(const BuildingCost& cost, char* text, size_t size) {
    std::array<int, MATERIAL_COUNT> materials = cost.Materials();
    int written = 0;
    text[0] = 0;
    for (int i = 0; i < MATERIAL_COUNT; i++) {
        if (materials[i] <= 0) continue;
        written += std::snprintf(text + written, size - written, "%s%d %s", written > 0 ? ", " : "", materials[i], ItemName(MATERIALS[i]));
        if (written >= (int)size) break;
    }
    return written > 0;
}

void HouseInfo(InfoLines& lines, GameObjectId id, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    char text[96];
    const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
    const PopulationTier& tier = POPULATION_TIERS[type.tier];
    const ResidenceComponent& residence = objects.Residence(id);
    const LogisticsComponent& logistics = objects.Logistics(id);

    InfoLine& residents = lines.Add(tier.name);
    residents.icon = TierIcon(type.tier);
    std::snprintf(text, sizeof(text), "%d / %d", residence.residents, tier.maxResidents);
    residents.value = text;
    if (!logistics.connected) {
        lines.Add("No road to a warehouse", Tone::Bad).icon = "icons/warning.tga";
        return;
    }
    if (!logistics.InReach(ServiceType::Marketplace)) {
        lines.Add("No marketplace in reach", Tone::Warn).icon = "icons/warning.tga";
        return;
    }

    bool allMet = true;
    for (int n = 0; n < tier.needCount; n++) {
        const Need& need = tier.needs[n];
        int supply = residence.needSupply[n];
        if (supply < PopulationSystem::UPGRADE_SUPPLY) allMet = false;
        std::snprintf(text, sizeof(text), "%s (+%d residents)", NeedName(need), need.residentsGranted);
        InfoLine& line = lines.Add(text);
        line.icon = NeedIcon(need);
        std::snprintf(text, sizeof(text), "%d%%", supply / 10);
        line.value = text;
        line.bar = supply / 1000.0f;
    }

    // Upgrade progress, or what is missing
    if (type.tier + 1 >= TIER_COUNT) return;
    const char* next = POPULATION_TIERS[type.tier + 1].name;
    if (residence.residents < tier.maxResidents) {
        lines.Add("Upgrade: needs a full house", Tone::Muted);
    } else if (!allMet) {
        std::snprintf(text, sizeof(text), "Upgrade: needs every need at %d%%", PopulationSystem::UPGRADE_SUPPLY / 10);
        lines.Add(text, Tone::Muted);
    } else if (!PopulationSystem::IsReadyToUpgrade(objects, id)) {
        std::snprintf(text, sizeof(text), "Upgrade to %s: getting ready", next);
        InfoLine& line = lines.Add(text);
        line.bar = (float)residence.upgradeTicks / PopulationSystem::UPGRADE_TICKS;
    } else if (!PopulationSystem::CanUpgrade(objects, economy, id)) {
        char cost[64];
        MaterialsText(UpgradeCost(type.tier), cost, sizeof(cost));
        std::snprintf(text, sizeof(text), "Ready to upgrade, needs %s", cost);
        lines.Add(text, Tone::Warn).icon = "icons/upgrade.tga";
    } else {
        std::snprintf(text, sizeof(text), "Ready to upgrade to %s", next);
        lines.Add(text, Tone::Good).icon = "icons/upgrade.tga";
    }
}

void ServiceInfo(InfoLines& lines, GameObjectId id, const GameObjectRegistry& objects) {
    if (!objects.Logistics(id).connected) {
        lines.Add("No road to a warehouse: serves nobody", Tone::Bad).icon = "icons/warning.tga";
        return;
    }
    ServiceType service = BUILDING_TYPES[objects.Building(id).type].service;
    int houses = 0, residents = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId other = objects.IdAtSlot(slot);
        if (other == INVALID_GAME_OBJECT || objects.Logistics(other).services[(size_t)service] != id) continue;
        if (BUILDING_TYPES[objects.Building(other).type].role != BuildingRole::Residence) continue;
        houses++;
        residents += objects.Residence(other).residents;
    }
    lines.Add("Houses served").value = Rml::ToString(houses);
    InfoLine& line = lines.Add("Residents");
    line.icon = "icons/residents.tga";
    line.value = Rml::ToString(residents);
}

const char* StatusText(ProducerStatus status) {
    switch (status) {
    case ProducerStatus::Working: return "Working";
    case ProducerStatus::NoRoad: return "No road to a warehouse";
    case ProducerStatus::NoWorkforce: return "No workers";
    case ProducerStatus::BadLocation: return "Nothing to work with here";
    case ProducerStatus::MissingInput: return "Waiting for input";
    case ProducerStatus::OutputFull: return "Storage full: waiting for a cart";
    }
    return "?";
}

// Where the producer's cart is and what it carries
void CartInfo(InfoLines& lines, const ProductionChain& chain, const ProductionComponent& production) {
    char text[96];
    // The inputs it brings back, e.g. "3 Wood"
    char inputs[64] = "nothing";
    int written = 0;
    for (int i = 0; i < chain.inputCount; i++) {
        if (production.cartInputs[i] == 0) continue;
        written += std::snprintf(inputs + written, sizeof(inputs) - written, "%s%d %s", written > 0 ? ", " : "", production.cartInputs[i],
            ItemName(chain.inputs[i]));
    }

    Tone tone = Tone::Normal;
    switch (production.cartState) {
    case CartState::Idle:
        if (production.cartWaitTicks > 0) {
            std::snprintf(text, sizeof(text), "Cart: at home, sets out in %d s", (CART_MAX_WAIT_TICKS - production.cartWaitTicks + 9) / 10);
        } else {
            std::snprintf(text, sizeof(text), "Cart: at home");
            tone = Tone::Muted;
        }
        break;
    case CartState::ToWarehouse:
        if (production.cartOutput > 0) std::snprintf(text, sizeof(text), "Cart: to the warehouse with %d %s", production.cartOutput, ItemName(chain.output));
        else std::snprintf(text, sizeof(text), "Cart: to the warehouse to fetch %s", chain.inputCount > 0 ? ItemName(chain.inputs[0]) : "goods");
        break;
    case CartState::Unloading:
        if (production.cartWaitTicks >= CART_UNLOAD_TICKS && production.cartOutput > 0) {
            std::snprintf(text, sizeof(text), "Cart: waiting, the warehouse is full");
            tone = Tone::Warn;
        } else {
            std::snprintf(text, sizeof(text), "Cart: unloading at the warehouse");
        }
        break;
    case CartState::ToProducer:
        if (production.cartOutput > 0) {
            std::snprintf(text, sizeof(text), "Cart: road cut, coming back with %d %s", production.cartOutput, ItemName(chain.output));
            tone = Tone::Warn;
        } else {
            std::snprintf(text, sizeof(text), "Cart: coming back with %s", inputs);
        }
        break;
    }
    lines.Add(text, tone);
}

void ProducerInfo(InfoLines& lines, GameObjectId id, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    char text[96];
    const ProductionChain& chain = PRODUCTION_CHAINS[BUILDING_TYPES[objects.Building(id).type].chain];
    const ProductionComponent& production = objects.Production(id);
    if (chain.inputCount == 0) std::snprintf(text, sizeof(text), "Makes %s", ItemName(chain.output));
    else if (chain.inputCount == 1) std::snprintf(text, sizeof(text), "Makes %s from %s", ItemName(chain.output), ItemName(chain.inputs[0]));
    else std::snprintf(text, sizeof(text), "Makes %s from %s and %s", ItemName(chain.output), ItemName(chain.inputs[0]), ItemName(chain.inputs[1]));
    lines.Add(text).icon = ItemIcon(chain.output);

    Tone tone = production.status == ProducerStatus::Working ? Tone::Good
              : (production.status == ProducerStatus::OutputFull || production.status == ProducerStatus::MissingInput) ? Tone::Warn : Tone::Bad;
    int module = ModuleTypeOf(objects.Building(id).type);
    if (module >= 0 && production.status == ProducerStatus::BadLocation) std::snprintf(text, sizeof(text), "Needs %s modules around it", BUILDING_TYPES[module].name);
    else std::snprintf(text, sizeof(text), "%s", StatusText(production.status));
    InfoLine& status = lines.Add(text, tone);
    if (tone != Tone::Good) status.icon = "icons/warning.tga";
    if (module >= 0) {
        // Its modules, as in Anno: productivity grows with each up to the full count
        int count = CountModules(objects, id);
        std::snprintf(text, sizeof(text), "%s modules", BUILDING_TYPES[module].name);
        InfoLine& line = lines.Add(text);
        std::snprintf(text, sizeof(text), "%d / %d", count, chain.fullSpeedCount);
        line.value = text;
        line.bar = (float)count / chain.fullSpeedCount;
        line.tone = (int)(count >= chain.fullSpeedCount ? Tone::Good : Tone::Warn);
    }

    // Productivity: workforce share x location factor, and the cycle under way
    const IslandStorage* storage = economy.Find(objects.Building(id).island);
    int workforce = storage ? storage->workforce[chain.workforceTier] : 0;
    InfoLine& productivity = lines.Add("Productivity");
    std::snprintf(text, sizeof(text), "%d%%", production.productivity / 10);
    productivity.value = text;
    productivity.bar = std::min(1.0f, production.progress / ((float)chain.cycleTicks * 1000.0f));
    std::snprintf(text, sizeof(text), "Workers %d%%, location %d%%", workforce / 10, production.locationFactor / 10);
    lines.Add(text, Tone::Muted);
    std::snprintf(text, sizeof(text), "%d %s workers, cycle %d s", chain.workforce, POPULATION_TIERS[chain.workforceTier].name, chain.cycleTicks / 10);
    lines.Add(text, Tone::Muted);

    for (int i = 0; i < chain.inputCount; i++) {
        std::snprintf(text, sizeof(text), "In: %s", ItemName(chain.inputs[i]));
        InfoLine& line = lines.Add(text);
        line.icon = ItemIcon(chain.inputs[i]);
        std::snprintf(text, sizeof(text), "%d / %d", production.inputs[i], PRODUCER_BUFFER);
        line.value = text;
    }
    std::snprintf(text, sizeof(text), "Out: %s (made %u)", ItemName(chain.output), production.cycles);
    InfoLine& output = lines.Add(text);
    output.icon = ItemIcon(chain.output);
    std::snprintf(text, sizeof(text), "%d / %d", production.output, PRODUCER_BUFFER);
    output.value = text;
    CartInfo(lines, chain, production);
}

void WarehouseInfo(InfoLines& lines, GameObjectId id, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    const IslandStorage* storage = economy.Find(objects.Building(id).island);
    if (!storage) return;
    char text[64];
    std::snprintf(text, sizeof(text), "Island storage: %d per good", storage->CapacityPerItem());
    lines.Add(text, Tone::Heading);
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (storage->amounts[i] == 0) continue;
        InfoLine& line = lines.Add(ITEM_NAMES[i]);
        line.icon = ItemIcon((ItemType)i);
        line.value = Rml::ToString(storage->amounts[i]);
    }
}

// The building's details, for the tooltip and the panel
void BuildingDetails(InfoLines& lines, GameObjectId building, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    const BuildingType& type = BUILDING_TYPES[objects.Building(building).type];
    switch (type.role) {
    case BuildingRole::Residence: HouseInfo(lines, building, objects, economy); break;
    case BuildingRole::Service: ServiceInfo(lines, building, objects); break;
    case BuildingRole::Storage: WarehouseInfo(lines, building, objects, economy); break;
    case BuildingRole::Producer: ProducerInfo(lines, building, objects, economy); break;
    case BuildingRole::Module: {
        char text[64];
        GameObjectId farm = objects.Building(building).owner;
        if (objects.IsAlive(farm)) {
            std::snprintf(text, sizeof(text), "Belongs to a %s", BUILDING_TYPES[objects.Building(farm).type].name);
            lines.Add(text, Tone::Good);
        } else {
            std::snprintf(text, sizeof(text), "No %s: this does nothing", BUILDING_TYPES[type.moduleOf].name);
            lines.Add(text, Tone::Bad).icon = "icons/warning.tga";
        }
        break;
    }
    }

    // Money
    char text[64];
    const BuildingCost& cost = BUILDING_COSTS[objects.Building(building).type];
    InfoLine& money = lines.Add(type.role == BuildingRole::Residence ? "Taxes" : "Upkeep");
    money.icon = "icons/coin.tga";
    if (type.role == BuildingRole::Residence) {
        int64_t tax = Treasury::HouseTaxMilli(objects, building);
        std::snprintf(text, sizeof(text), "+%lld.%lld / min", (long long)(tax / 1000), (long long)(tax % 1000 / 100));
        money.tone = (int)Tone::Good;
    } else {
        std::snprintf(text, sizeof(text), cost.upkeep > 0 ? "-%d / min" : "none", cost.upkeep);
    }
    money.value = text;
    BuildingCost refund = { (int16_t)(cost.coins / 2), (int16_t)(cost.planks / 2), 0, (int16_t)(cost.bricks / 2), (int16_t)(cost.steelBeams / 2) };
    char materials[64];
    if (MaterialsText(refund, materials, sizeof(materials))) std::snprintf(text, sizeof(text), "Demolish refunds %d coins, %s", refund.coins, materials);
    else std::snprintf(text, sizeof(text), "Demolish refunds %d coins", refund.coins);
    if (cost.coins > 0) lines.Add(text, Tone::Muted);
}

void Show(Rml::ElementDocument* document, bool visible) {
    if (!document || document->IsVisible() == visible) return;
    if (visible) {
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        document->PullToFront(); // Over the menus
    } else {
        document->Hide();
    }
}

template <typename T>
void SetIfChanged(T& field, const T& value, Rml::DataModelHandle model, const char* name) {
    if (field == value) return;
    field = value;
    model.DirtyVariable(name);
}

} // namespace

bool BuildingInfo::Init(Rml::Context* context) {
    Rml::DataModelConstructor tooltip = context->CreateDataModel("building_tooltip");
    if (!tooltip) return false;
    tooltip.Bind("title", &m_TooltipTitle);
    m_TooltipLines.Bind(tooltip, "lines");
    m_TooltipModel = tooltip.GetModelHandle();

    Rml::DataModelConstructor panel = context->CreateDataModel("building_panel");
    if (!panel) return false;
    panel.Bind("title", &m_PanelTitle);
    m_PanelLines.Bind(panel, "lines");
    if (Rml::StructHandle<Action> action = panel.RegisterStruct<Action>()) {
        action.RegisterMember("label", &Action::label);
        action.RegisterMember("enabled", &Action::enabled);
        action.RegisterMember("id", &Action::id);
    }
    panel.RegisterArray<std::vector<Action>>();
    panel.Bind("actions", &m_Actions);
    panel.BindEventCallback("do_action", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_ActionRequest = arguments[0].Get<int>();
    });
    panel.BindEventCallback("close", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_CloseRequest = true; });
    m_PanelModel = panel.GetModelHandle();
    m_Actions.reserve(3);

    m_Tooltip = context->LoadDocument("assets/ui/building_tooltip.rml");
    m_Panel = context->LoadDocument("assets/ui/building_panel.rml");
    return m_Tooltip && m_Panel;
}

void BuildingInfo::Update(GameObjectId tooltip, GameObjectId panel, const Simulation& simulation, glm::vec2 cursor, glm::ivec2 size) {
    const GameObjectRegistry& objects = simulation.Objects();
    const IslandEconomyManager& economy = simulation.Economy();

    // Tooltip: below and right of the cursor, kept on the screen
    bool showTooltip = objects.IsAlive(tooltip);
    Show(m_Tooltip, showTooltip);
    if (showTooltip) {
        SetIfChanged(m_TooltipTitle, Rml::String(BUILDING_TYPES[objects.Building(tooltip).type].name), m_TooltipModel, "title");
        m_TooltipLines.Begin();
        BuildingDetails(m_TooltipLines, tooltip, objects, economy);
        m_TooltipLines.End(m_TooltipModel);
        Rml::Vector2f box = m_Tooltip->GetBox().GetSize(Rml::BoxArea::Border);
        float x = std::min(cursor.x + 20.0f, size.x - box.x - 8.0f);
        float y = std::min(cursor.y + 20.0f, size.y - box.y - 8.0f);
        m_Tooltip->SetProperty("left", Rml::ToString(std::max(0.0f, x)) + "px");
        m_Tooltip->SetProperty("top", Rml::ToString(std::max(0.0f, y)) + "px");
    }

    // Panel of the clicked building
    bool showPanel = objects.IsAlive(panel);
    Show(m_Panel, showPanel);
    m_PanelBuilding = showPanel ? panel : INVALID_GAME_OBJECT;
    if (!showPanel) return;
    const BuildingType& type = BUILDING_TYPES[objects.Building(panel).type];
    SetIfChanged(m_PanelTitle, Rml::String(type.name), m_PanelModel, "title");
    m_PanelLines.Begin();
    BuildingDetails(m_PanelLines, panel, objects, economy);

    char text[64];
    Action actions[3];
    int count = 0;
    if (type.role == BuildingRole::Residence && type.tier + 1 < TIER_COUNT) {
        char cost[64];
        MaterialsText(UpgradeCost(type.tier), cost, sizeof(cost));
        std::snprintf(text, sizeof(text), "Upgrade to %s (%s)", POPULATION_TIERS[type.tier + 1].name, cost);
        actions[count++] = { text, PopulationSystem::CanUpgrade(objects, economy, panel), UPGRADE };
    }
    if (int module = ModuleTypeOf(objects.Building(panel).type); module >= 0) {
        // Farms: place their modules
        const ProductionChain& chain = PRODUCTION_CHAINS[type.chain];
        std::snprintf(text, sizeof(text), "Build %s (%d coins)", BUILDING_TYPES[module].name, BUILDING_COSTS[module].coins);
        actions[count++] = { text, CountModules(objects, panel) < chain.fullSpeedCount, BUILD_MODULE };
    }
    if (type.role == BuildingRole::Storage && type.dockRows > 0) {
        // Harbors: ships are built here
        const IslandStorage* storage = economy.Find(objects.Building(panel).island);
        bool can = storage && storage->Amount(ItemType::Planks) >= ShipSystem::SHIP_PLANKS && simulation.Coins().Coins() >= ShipSystem::SHIP_COINS;
        std::snprintf(text, sizeof(text), "Build ship (%d coins, %d planks)", ShipSystem::SHIP_COINS, ShipSystem::SHIP_PLANKS);
        actions[count++] = { text, can, BUILD_SHIP };
        int docked = 0;
        const ShipSystem& ships = simulation.Ships();
        for (int slot = 0; slot < ShipSystem::MAX_SHIPS; slot++) {
            ShipId ship = ships.IdAtSlot(slot);
            if (ship != INVALID_SHIP && ships.Get(ship).state == ShipState::Docked && ships.Get(ship).harbor == panel) docked++;
        }
        InfoLine& line = m_PanelLines.Add("Ships docked here");
        line.icon = "icons/ship.tga";
        line.value = Rml::ToString(docked);
    }
    m_PanelLines.End(m_PanelModel);

    bool same = (int)m_Actions.size() == count;
    for (int i = 0; same && i < count; i++) same = m_Actions[i] == actions[i];
    if (!same) {
        m_Actions.assign(actions, actions + count); // Within the reserved capacity
        m_PanelModel.DirtyVariable("actions");
    }
}

void BuildingInfo::ApplyRequests(Simulation& simulation, BuildTool& tool) {
    if (m_CloseRequest) tool.ClearInspection();
    // Only an action the panel shows as possible (a click on a greyed-out button does nothing)
    bool enabled = false;
    for (const Action& action : m_Actions) enabled |= action.id == m_ActionRequest && action.enabled;
    if (enabled && simulation.Objects().IsAlive(m_PanelBuilding)) {
        if (m_ActionRequest == UPGRADE) simulation.Population().RequestUpgrade(m_PanelBuilding);
        if (m_ActionRequest == BUILD_SHIP) simulation.Ships().Build(m_PanelBuilding, simulation.Objects(), simulation.Economy(), simulation.Coins());
        if (m_ActionRequest == BUILD_MODULE) tool.SelectModules(m_PanelBuilding);
    }
    m_ActionRequest = 0;
    m_CloseRequest = false;
}
