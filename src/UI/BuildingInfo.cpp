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

void UpgradeLines(InfoLines& lines, GameObjectId id, const GameObjectRegistry& objects, const IslandEconomyManager& economy);

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

    for (int n = 0; n < tier.needCount; n++) {
        const Need& need = tier.needs[n];
        int supply = residence.needSupply[n];
        std::snprintf(text, sizeof(text), "%s (+%d residents)", NeedName(need), need.residentsGranted);
        InfoLine& line = lines.Add(text);
        line.icon = NeedIcon(need);
        std::snprintf(text, sizeof(text), "%d%%", supply / 10);
        line.value = text;
        line.bar = supply / 1000.0f;
    }
    UpgradeLines(lines, id, objects, economy);
}

// A house's upgrade progress, or what it is missing
void UpgradeLines(InfoLines& lines, GameObjectId id, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    char text[96];
    const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
    const PopulationTier& tier = POPULATION_TIERS[type.tier];
    const ResidenceComponent& residence = objects.Residence(id);
    bool allMet = true;
    for (int n = 0; n < tier.needCount; n++) allMet &= residence.needSupply[n] >= PopulationSystem::UPGRADE_SUPPLY;
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
    case ProducerStatus::Paused: return "Paused";
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

void MoneyLines(InfoLines& lines, GameObjectId building, const GameObjectRegistry& objects);

// The building's details, for the tooltip
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
    MoneyLines(lines, building, objects);
}

// Taxes or upkeep, and what demolishing gives back
void MoneyLines(InfoLines& lines, GameObjectId building, const GameObjectRegistry& objects) {
    const BuildingType& type = BUILDING_TYPES[objects.Building(building).type];
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

int ToneOf(ProducerStatus status) {
    if (status == ProducerStatus::Working) return (int)Tone::Good;
    if (status == ProducerStatus::OutputFull || status == ProducerStatus::MissingInput || status == ProducerStatus::Paused) return (int)Tone::Warn;
    return (int)Tone::Bad;
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

void BuildingInfo::FillMenu(GameObjectId id, const Simulation& simulation) {
    const GameObjectRegistry& objects = simulation.Objects();
    const IslandEconomyManager& economy = simulation.Economy();
    const BuildingComponent& building = objects.Building(id);
    const BuildingType& type = BUILDING_TYPES[building.type];
    const LogisticsComponent& logistics = objects.Logistics(id);
    char text[96];

    Menu menu;
    menu.title = type.name;
    menu.image = Rml::String("buildings/") + (type.modelName ? type.modelName : "road") + ".tga";
    menu.canPause = type.role == BuildingRole::Producer;
    m_SlotScratch.clear();
    m_HistoryScratch.clear();
    m_MenuLines.Begin();
    auto slot = [this](const char* icon, Rml::String label, Rml::String value, float fill, Tone tone) {
        m_SlotScratch.push_back({ icon, std::move(label), std::move(value), fill, (int)tone }); // Within the reserve
    };

    switch (type.role) {
    case BuildingRole::Residence: {
        const PopulationTier& tier = POPULATION_TIERS[type.tier];
        const ResidenceComponent& residence = objects.Residence(id);
        menu.subtitle = tier.name;
        menu.subtitleTone = (int)Tone::Muted;
        if (!logistics.connected) {
            menu.subtitle = "No road to a warehouse";
            menu.subtitleTone = (int)Tone::Bad;
        } else if (!logistics.InReach(ServiceType::Marketplace)) {
            menu.subtitle = "No marketplace in reach";
            menu.subtitleTone = (int)Tone::Warn;
        }
        menu.bigLabel = "Residents";
        std::snprintf(text, sizeof(text), "%d / %d", residence.residents, tier.maxResidents);
        menu.bigValue = text;
        menu.bigBar = (float)residence.residents / tier.maxResidents;
        menu.slotsTitle = "Needs";
        for (int n = 0; n < tier.needCount; n++) {
            const Need& need = tier.needs[n];
            int supply = residence.needSupply[n];
            std::snprintf(text, sizeof(text), "%d%%", supply / 10);
            Tone tone = supply >= PopulationSystem::UPGRADE_SUPPLY ? Tone::Good : supply >= 500 ? Tone::Warn : Tone::Bad;
            slot(NeedIcon(need), Rml::String(NeedName(need)) + " +" + Rml::ToString((int)need.residentsGranted), text, supply / 1000.0f, tone);
        }
        UpgradeLines(m_MenuLines, id, objects, economy);
        break;
    }
    case BuildingRole::Producer: {
        const ProductionChain& chain = PRODUCTION_CHAINS[type.chain];
        const ProductionComponent& production = objects.Production(id);
        int module = ModuleTypeOf(building.type);
        menu.paused = production.paused;
        if (module >= 0 && production.status == ProducerStatus::BadLocation) {
            std::snprintf(text, sizeof(text), "Needs %s modules around it", BUILDING_TYPES[module].name);
            menu.subtitle = text;
        } else {
            menu.subtitle = StatusText(production.status);
        }
        menu.subtitleTone = ToneOf(production.status);
        menu.bigLabel = "Productivity";
        std::snprintf(text, sizeof(text), "%d%%", production.productivity / 10);
        menu.bigValue = text;
        menu.bigBar = std::min(1.0f, production.progress / ((float)chain.cycleTicks * 1000.0f)); // The cycle under way
        for (int i = 0; i < production.historyCount; i++) {
            int percent = production.history[(production.historyHead + i) % PRODUCTIVITY_SAMPLES];
            m_HistoryScratch.push_back({ Rml::ToString(std::max(1, percent / 2)) + "dp", percent }); // Within the reserve
        }
        menu.slotsTitle = "Storage";
        for (int i = 0; i < chain.inputCount; i++) {
            std::snprintf(text, sizeof(text), "%d / %d", production.inputs[i], PRODUCER_BUFFER);
            slot(ItemIcon(chain.inputs[i]), ItemName(chain.inputs[i]), text, (float)production.inputs[i] / PRODUCER_BUFFER,
                production.inputs[i] == 0 ? Tone::Bad : Tone::Normal);
        }
        std::snprintf(text, sizeof(text), "%d / %d", production.output, PRODUCER_BUFFER);
        slot(ItemIcon(chain.output), Rml::String("Makes ") + ItemName(chain.output), text, (float)production.output / PRODUCER_BUFFER,
            production.output >= PRODUCER_BUFFER ? Tone::Warn : Tone::Good);

        const IslandStorage* storage = economy.Find(building.island);
        int workforce = storage ? storage->workforce[chain.workforceTier] : 0;
        std::snprintf(text, sizeof(text), "%d %s, %d%% filled", chain.workforce, POPULATION_TIERS[chain.workforceTier].name, workforce / 10);
        InfoLine& workers = m_MenuLines.Add(text, workforce >= 1000 ? Tone::Normal : Tone::Warn);
        workers.icon = TierIcon(chain.workforceTier);
        std::snprintf(text, sizeof(text), "Location %d%%, cycle %d s, made %u", production.locationFactor / 10, chain.cycleTicks / 10, production.cycles);
        m_MenuLines.Add(text, Tone::Muted);
        if (module >= 0) {
            int count = CountModules(objects, id);
            std::snprintf(text, sizeof(text), "%s modules", BUILDING_TYPES[module].name);
            InfoLine& line = m_MenuLines.Add(text, count >= chain.fullSpeedCount ? Tone::Good : Tone::Warn);
            std::snprintf(text, sizeof(text), "%d / %d", count, chain.fullSpeedCount);
            line.value = text;
        }
        CartInfo(m_MenuLines, chain, production);
        break;
    }
    case BuildingRole::Storage: {
        const IslandStorage* storage = economy.Find(building.island);
        menu.subtitle = type.dockRows > 0 ? "Ships load and unload here" : "Carts bring the goods here";
        menu.subtitleTone = (int)Tone::Muted;
        menu.bigLabel = "Island storage";
        std::snprintf(text, sizeof(text), "%d", storage ? storage->CapacityPerItem() : 0);
        menu.bigValue = text;
        menu.bigUnit = "per good";
        menu.slotsTitle = "Inventory";
        for (int i = 0; storage && i < ITEM_COUNT; i++) {
            if (storage->amounts[i] == 0) continue;
            float full = (float)storage->amounts[i] / std::max(1, storage->CapacityPerItem());
            slot(ItemIcon((ItemType)i), ITEM_NAMES[i], Rml::ToString(storage->amounts[i]), full, full >= 1.0f ? Tone::Warn : Tone::Normal);
        }
        if (type.dockRows > 0) {
            int docked = 0;
            const ShipSystem& ships = simulation.Ships();
            for (int s = 0; s < ShipSystem::MAX_SHIPS; s++) {
                ShipId ship = ships.IdAtSlot(s);
                if (ship != INVALID_SHIP && ships.Get(ship).state == ShipState::Docked && ships.Get(ship).harbor == id) docked++;
            }
            InfoLine& line = m_MenuLines.Add("Ships docked here");
            line.icon = "icons/ship.tga";
            line.value = Rml::ToString(docked);
        }
        break;
    }
    case BuildingRole::Service: {
        int houses = 0, residents = 0;
        for (uint32_t s = 0; s < objects.SlotCount(); s++) {
            GameObjectId other = objects.IdAtSlot(s);
            if (other == INVALID_GAME_OBJECT || objects.Logistics(other).services[(size_t)type.service] != id) continue;
            if (BUILDING_TYPES[objects.Building(other).type].role != BuildingRole::Residence) continue;
            houses++;
            residents += objects.Residence(other).residents;
        }
        menu.subtitle = logistics.connected ? "Serves the houses in its reach" : "No road to a warehouse: serves nobody";
        menu.subtitleTone = logistics.connected ? (int)Tone::Muted : (int)Tone::Bad;
        menu.bigLabel = "Residents served";
        menu.bigValue = Rml::ToString(residents);
        std::snprintf(text, sizeof(text), "in %d house%s", houses, houses == 1 ? "" : "s");
        menu.bigUnit = text;
        std::snprintf(text, sizeof(text), "Reaches %d road tiles", SERVICE_ROAD_RANGE[(size_t)type.service]);
        m_MenuLines.Add(text, Tone::Muted);
        break;
    }
    case BuildingRole::Module: {
        GameObjectId farm = building.owner;
        bool owned = objects.IsAlive(farm);
        std::snprintf(text, sizeof(text), owned ? "Belongs to a %s" : "No %s: this does nothing", owned ? BUILDING_TYPES[objects.Building(farm).type].name
                                                                                                   : BUILDING_TYPES[type.moduleOf].name);
        menu.subtitle = text;
        menu.subtitleTone = owned ? (int)Tone::Good : (int)Tone::Bad;
        break;
    }
    }
    MoneyLines(m_MenuLines, id, objects);
    m_MenuLines.End(m_MenuModel);

    // Only what changed is marked dirty
    SetIfChanged(m_Menu.title, menu.title, m_MenuModel, "title");
    SetIfChanged(m_Menu.subtitle, menu.subtitle, m_MenuModel, "subtitle");
    SetIfChanged(m_Menu.subtitleTone, menu.subtitleTone, m_MenuModel, "subtitle_tone");
    SetIfChanged(m_Menu.image, menu.image, m_MenuModel, "image");
    SetIfChanged(m_Menu.bigLabel, menu.bigLabel, m_MenuModel, "big_label");
    SetIfChanged(m_Menu.bigValue, menu.bigValue, m_MenuModel, "big_value");
    SetIfChanged(m_Menu.bigUnit, menu.bigUnit, m_MenuModel, "big_unit");
    SetIfChanged(m_Menu.bigBar, menu.bigBar, m_MenuModel, "big_bar");
    SetIfChanged(m_Menu.slotsTitle, menu.slotsTitle, m_MenuModel, "slots_title");
    SetIfChanged(m_Menu.canPause, menu.canPause, m_MenuModel, "can_pause");
    SetIfChanged(m_Menu.paused, menu.paused, m_MenuModel, "paused");
    if (m_Slots != m_SlotScratch) {
        m_Slots = m_SlotScratch; // Within the reserved capacity
        m_MenuModel.DirtyVariable("slots");
    }
    if (m_History != m_HistoryScratch) {
        m_History = m_HistoryScratch;
        m_MenuModel.DirtyVariable("history");
    }
}

bool BuildingInfo::Init(Rml::Context* context) {
    Rml::DataModelConstructor tooltip = context->CreateDataModel("building_tooltip");
    if (!tooltip) return false;
    tooltip.Bind("title", &m_TooltipTitle);
    m_TooltipLines.Bind(tooltip, "lines");
    m_TooltipModel = tooltip.GetModelHandle();

    Rml::DataModelConstructor panel = context->CreateDataModel("object_menu");
    if (!panel) return false;
    panel.Bind("title", &m_Menu.title);
    panel.Bind("subtitle", &m_Menu.subtitle);
    panel.Bind("subtitle_tone", &m_Menu.subtitleTone);
    panel.Bind("image", &m_Menu.image);
    panel.Bind("big_label", &m_Menu.bigLabel);
    panel.Bind("big_value", &m_Menu.bigValue);
    panel.Bind("big_unit", &m_Menu.bigUnit);
    panel.Bind("big_bar", &m_Menu.bigBar);
    panel.Bind("slots_title", &m_Menu.slotsTitle);
    panel.Bind("can_pause", &m_Menu.canPause);
    panel.Bind("paused", &m_Menu.paused);
    m_MenuLines.Bind(panel, "lines");
    if (Rml::StructHandle<Slot> slot = panel.RegisterStruct<Slot>()) {
        slot.RegisterMember("icon", &Slot::icon);
        slot.RegisterMember("label", &Slot::label);
        slot.RegisterMember("value", &Slot::value);
        slot.RegisterMember("fill", &Slot::fill);
        slot.RegisterMember("tone", &Slot::tone);
    }
    panel.RegisterArray<std::vector<Slot>>();
    panel.Bind("slots", &m_Slots);
    if (Rml::StructHandle<Bar> bar = panel.RegisterStruct<Bar>()) {
        bar.RegisterMember("height", &Bar::height);
        bar.RegisterMember("percent", &Bar::percent);
    }
    panel.RegisterArray<std::vector<Bar>>();
    panel.Bind("history", &m_History);
    panel.BindEventCallback("tool", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
        if (!arguments.empty()) m_ToolRequest = arguments[0].Get<int>();
    });
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
    m_MenuModel = panel.GetModelHandle();
    m_Actions.reserve(3);
    for (std::vector<Slot>* slots : { &m_Slots, &m_SlotScratch }) slots->reserve(ITEM_COUNT);
    for (std::vector<Bar>* bars : { &m_History, &m_HistoryScratch }) bars->reserve(PRODUCTIVITY_SAMPLES);

    m_Tooltip = context->LoadDocument("assets/ui/building_tooltip.rml");
    m_Panel = context->LoadDocument("assets/ui/object_menu.rml");
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

    // The object menu of the clicked building
    bool showPanel = objects.IsAlive(panel);
    Show(m_Panel, showPanel);
    m_PanelBuilding = showPanel ? panel : INVALID_GAME_OBJECT;
    if (!showPanel) return;
    FillMenu(panel, simulation);

    const BuildingType& type = BUILDING_TYPES[objects.Building(panel).type];
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
    }
    bool same = (int)m_Actions.size() == count;
    for (int i = 0; same && i < count; i++) same = m_Actions[i] == actions[i];
    if (!same) {
        m_Actions.assign(actions, actions + count); // Within the reserved capacity
        m_MenuModel.DirtyVariable("actions");
    }
}

void BuildingInfo::ApplyRequests(Simulation& simulation, BuildTool& tool) {
    if (m_CloseRequest) tool.ClearInspection();
    // The header's tools: move, copy, pause, demolish
    GameObjectId id = m_PanelBuilding;
    if (m_ToolRequest != 0 && simulation.Objects().IsAlive(id)) {
        if (m_ToolRequest == TOOL_MOVE) {
            tool.ClearInspection();
            tool.BeginMove(id);
        } else if (m_ToolRequest == TOOL_COPY) {
            tool.ClearInspection();
            tool.CopyBuilding(id);
        } else if (m_ToolRequest == TOOL_PAUSE && BUILDING_TYPES[simulation.Objects().Building(id).type].role == BuildingRole::Producer) {
            bool& paused = simulation.Objects().Production(id).paused;
            paused = !paused;
        } else if (m_ToolRequest == TOOL_DEMOLISH) {
            tool.ClearInspection();
            tool.Demolish(id);
        }
    }
    m_ToolRequest = 0;
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
