#include "Simulation/GameObjects.h"

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
