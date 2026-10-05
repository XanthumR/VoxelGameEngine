#include "UI/BuildingInfo.h"

#include "Economy/IslandEconomy.h"
#include "Economy/PopulationSystem.h"
#include "Economy/ProductionChains.h"
#include "Simulation/BuildingTypes.h"

#include "imgui.h"

namespace {

const ImVec4 GOOD_COLOR(0.5f, 1.0f, 0.55f, 1.0f);
const ImVec4 WARNING_COLOR(1.0f, 0.85f, 0.3f, 1.0f);
const ImVec4 BAD_COLOR(1.0f, 0.45f, 0.4f, 1.0f);

void HouseInfo(GameObjectId id, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    const BuildingType& type = BUILDING_TYPES[objects.Building(id).type];
    const PopulationTier& tier = POPULATION_TIERS[type.tier];
    const ResidenceComponent& residence = objects.Residence(id);
    const LogisticsComponent& logistics = objects.Logistics(id);

    ImGui::Text("%s: %d / %d residents", tier.name, residence.residents, tier.maxResidents);
    if (!logistics.connected) {
        ImGui::TextColored(BAD_COLOR, "No road to a warehouse");
        return;
    }
    if (!logistics.inMarketRange) {
        ImGui::TextColored(WARNING_COLOR, "No marketplace in reach");
        return;
    }

    bool allMet = true;
    for (int n = 0; n < tier.needCount; n++) {
        int supply = residence.needSupply[n];
        if (supply < PopulationSystem::UPGRADE_SUPPLY) allMet = false;
        ImVec4 color = supply >= PopulationSystem::UPGRADE_SUPPLY ? GOOD_COLOR : (supply > 0 ? WARNING_COLOR : BAD_COLOR);
        ImGui::TextColored(color, "  %-13s %3d%%  (+%d residents)", tier.needs[n].name, supply / 10, tier.needs[n].residentsGranted);
    }

    // Upgrade progress, or what is missing
    if (type.tier + 1 >= TIER_COUNT) return;
    const IslandStorage* storage = economy.Find(objects.Building(id).island);
    if (residence.residents < tier.maxResidents) {
        ImGui::TextDisabled("Upgrade: needs a full house");
    } else if (!allMet) {
        ImGui::TextDisabled("Upgrade: needs every need at %d%%", PopulationSystem::UPGRADE_SUPPLY / 10);
    } else if (!storage || storage->Amount(ItemType::Planks) < UPGRADE_PLANKS) {
        ImGui::TextColored(WARNING_COLOR, "Upgrade: needs %d planks", UPGRADE_PLANKS);
    } else {
        ImGui::Text("Upgrade to %s: %d%%", POPULATION_TIERS[type.tier + 1].name,
            residence.upgradeTicks * 100 / PopulationSystem::UPGRADE_TICKS);
    }
}

void MarketInfo(GameObjectId id, const GameObjectRegistry& objects) {
    if (!objects.Logistics(id).connected) {
        ImGui::TextColored(BAD_COLOR, "No road to a warehouse: serves nobody");
        return;
    }
    int houses = 0, residents = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId other = objects.IdAtSlot(slot);
        if (other == INVALID_GAME_OBJECT || objects.Logistics(other).market != id) continue;
        if (BUILDING_TYPES[objects.Building(other).type].role != BuildingRole::Residence) continue;
        houses++;
        residents += objects.Residence(other).residents;
    }
    ImGui::Text("Serves %d houses, %d residents", houses, residents);
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

// What the producer makes, how well it is doing, and why not
void ProducerInfo(GameObjectId id, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    const ProductionChain& chain = PRODUCTION_CHAINS[BUILDING_TYPES[objects.Building(id).type].chain];
    const ProductionComponent& production = objects.Production(id);
    if (chain.inputCount == 0) ImGui::Text("Makes %s", ItemName(chain.output));
    else ImGui::Text("Makes %s from %s", ItemName(chain.output), ItemName(chain.inputs[0]));

    ImVec4 color = production.status == ProducerStatus::Working ? GOOD_COLOR
                 : (production.status == ProducerStatus::OutputFull || production.status == ProducerStatus::MissingInput) ? WARNING_COLOR : BAD_COLOR;
    ImGui::TextColored(color, "%s", StatusText(production.status));

    // Productivity: workforce share x location factor
    const IslandStorage* storage = economy.Find(objects.Building(id).island);
    int workforce = storage ? storage->workforce[chain.workforceTier] : 0;
    ImGui::Text("Productivity %d%% (workers %d%%, location %d%%)", production.productivity / 10, workforce / 10, production.locationFactor / 10);
    ImGui::Text("%d %s workers needed, cycle %d s", chain.workforce, POPULATION_TIERS[chain.workforceTier].name, chain.cycleTicks / 10);

    float cycle = (float)chain.cycleTicks * 1000.0f;
    ImGui::ProgressBar(production.progress / cycle, ImVec2(220.0f, 0.0f), "");
    for (int i = 0; i < chain.inputCount; i++) {
        ImGui::Text("In:  %-12s %d / %d", ItemName(chain.inputs[i]), production.inputs[i], PRODUCER_BUFFER);
    }
    ImGui::Text("Out: %-12s %d / %d   (made %u)", ItemName(chain.output), production.output, PRODUCER_BUFFER, production.cycles);
}

void WarehouseInfo(GameObjectId id, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    const IslandStorage* storage = economy.Find(objects.Building(id).island);
    if (!storage) return;
    ImGui::Text("Island storage: %d per good", storage->CapacityPerItem());
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (storage->amounts[i] > 0) ImGui::Text("  %-13s %d", ITEM_NAMES[i], storage->amounts[i]);
    }
}

} // namespace

void DrawBuildingInfo(GameObjectId building, const GameObjectRegistry& objects, const IslandEconomyManager& economy) {
    if (!objects.IsAlive(building)) return;
    const BuildingType& type = BUILDING_TYPES[objects.Building(building).type];

    ImGui::BeginTooltip();
    ImGui::TextUnformatted(type.name);
    ImGui::Separator();
    switch (type.role) {
    case BuildingRole::Residence: HouseInfo(building, objects, economy); break;
    case BuildingRole::Market: MarketInfo(building, objects); break;
    case BuildingRole::Storage: WarehouseInfo(building, objects, economy); break;
    case BuildingRole::Producer: ProducerInfo(building, objects, economy); break;
    }
    ImGui::EndTooltip();
}
