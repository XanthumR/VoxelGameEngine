#include "Simulation/GameObjects.h"

#include "Simulation/SaveGame.h"

#include <algorithm>

static_assert(GameObjectRegistry::MAX_OBJECTS <= 0x10000, "Slot must fit in the low 16 bits of an ID");

GameObjectRegistry::GameObjectRegistry()
    : m_Buildings(MAX_OBJECTS), m_Anchors(MAX_OBJECTS), m_Logistics(MAX_OBJECTS), m_Residences(MAX_OBJECTS), m_Production(MAX_OBJECTS), m_Generations(MAX_OBJECTS, 1), m_Alive(MAX_OBJECTS, 0) {
    m_FreeSlots.reserve(MAX_OBJECTS);
}

GameObjectId GameObjectRegistry::Create() {
    uint32_t slot;
    if (!m_FreeSlots.empty()) {
        slot = m_FreeSlots.back();
        m_FreeSlots.pop_back();
    } else if (m_UsedSlots < MAX_OBJECTS) {
        slot = m_UsedSlots++;
    } else {
        return INVALID_GAME_OBJECT;
    }

    m_Alive[slot] = 1;
    m_Buildings[slot] = BuildingComponent();
    m_Anchors[slot] = VoxelAnchorComponent();
    m_Logistics[slot] = LogisticsComponent();
    m_Residences[slot] = ResidenceComponent();
    m_Production[slot] = ProductionComponent();
    m_AliveCount++;
    return MakeId(slot, m_Generations[slot]);
}

bool GameObjectRegistry::IsAlive(GameObjectId id) const {
    uint32_t slot = SlotOf(id);
    return id != INVALID_GAME_OBJECT && slot < m_UsedSlots && m_Alive[slot] && m_Generations[slot] == (id >> 16);
}

bool GameObjectRegistry::Destroy(GameObjectId id) {
    if (!IsAlive(id)) return false;
    uint32_t slot = SlotOf(id);
    m_Alive[slot] = 0;
    // Skip 0 on wrap-around so no ID is ever 0
    m_Generations[slot] = m_Generations[slot] == 0xFFFF ? 1 : (uint16_t)(m_Generations[slot] + 1);
    m_FreeSlots.push_back(slot); // Within the reserved capacity
    m_AliveCount--;
    return true;
}

GameObjectId GameObjectRegistry::IdAtSlot(uint32_t slot) const {
    if (slot >= m_UsedSlots || !m_Alive[slot]) return INVALID_GAME_OBJECT;
    return MakeId(slot, m_Generations[slot]);
}

void GameObjectRegistry::Save(SaveWriter& out) const {
    out.Write(m_UsedSlots);
    out.Write(m_AliveCount);
    out.WriteArray(m_Generations.data(), m_UsedSlots);
    out.WriteArray(m_Alive.data(), m_UsedSlots);
    out.WriteArray(m_Buildings.data(), m_UsedSlots);
    out.WriteArray(m_Anchors.data(), m_UsedSlots);
    out.WriteArray(m_Logistics.data(), m_UsedSlots);
    out.WriteArray(m_Residences.data(), m_UsedSlots);
    out.WriteArray(m_Production.data(), m_UsedSlots);
    out.WriteVector(m_FreeSlots);
}

void GameObjectRegistry::Load(SaveReader& in) {
    uint32_t used = in.Read<uint32_t>();
    if (used > MAX_OBJECTS) {
        in.Fail();
        used = 0;
    }
    std::fill(m_Generations.begin(), m_Generations.end(), (uint16_t)1);
    std::fill(m_Alive.begin(), m_Alive.end(), (uint8_t)0);
    m_UsedSlots = used;
    m_AliveCount = in.Read<uint32_t>();
    in.ReadArray(m_Generations.data(), used);
    in.ReadArray(m_Alive.data(), used);
    in.ReadArray(m_Buildings.data(), used);
    in.ReadArray(m_Anchors.data(), used);
    in.ReadArray(m_Logistics.data(), used);
    in.ReadArray(m_Residences.data(), used);
    in.ReadArray(m_Production.data(), used);
    in.ReadVector(m_FreeSlots, MAX_OBJECTS); // Within the reserve
}
