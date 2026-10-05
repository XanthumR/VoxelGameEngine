#pragma once

#include "Simulation/IslandRegistry.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// Stable handle to a game object: low 16 bits are the slot, high 16 bits the slot's generation.
// Destroying an object bumps its slot's generation, so an old ID never refers to the object that
// later reuses the slot. 0 is never a valid ID.
using GameObjectId = uint32_t;
constexpr GameObjectId INVALID_GAME_OBJECT = 0;

struct BuildingComponent {
    uint16_t type = 0;          // Index into the building type table
    IslandId island = NO_ISLAND;
    uint8_t rotation = 0;       // Quarter turns
};

// Links an object to the voxels it occupies; the voxels are only its look
struct VoxelAnchorComponent {
    glm::ivec3 origin = glm::ivec3(0);    // Minimum corner (x, ground y, z)
    glm::ivec2 footprint = glm::ivec2(1); // Columns along x and z
};

// Owns every game object. Components live in flat arrays indexed by slot, all allocated up front,
// so creating and destroying objects never allocates and never moves components.
class GameObjectRegistry {
public:
    static constexpr uint32_t MAX_OBJECTS = 4096;

    GameObjectRegistry();

    // INVALID_GAME_OBJECT when all MAX_OBJECTS slots are in use
    GameObjectId Create();
    bool Destroy(GameObjectId id); // False for a dead or stale ID
    bool IsAlive(GameObjectId id) const;
    uint32_t AliveCount() const { return m_AliveCount; }

    // The ID must be alive
    BuildingComponent& Building(GameObjectId id) { return m_Buildings[SlotOf(id)]; }
    const BuildingComponent& Building(GameObjectId id) const { return m_Buildings[SlotOf(id)]; }
    VoxelAnchorComponent& Anchor(GameObjectId id) { return m_Anchors[SlotOf(id)]; }
    const VoxelAnchorComponent& Anchor(GameObjectId id) const { return m_Anchors[SlotOf(id)]; }

    // Iteration: slots below SlotCount() may hold an object; IdAtSlot is INVALID for empty ones
    uint32_t SlotCount() const { return m_UsedSlots; }
    GameObjectId IdAtSlot(uint32_t slot) const;

private:
    static uint32_t SlotOf(GameObjectId id) { return id & 0xFFFF; }
    static GameObjectId MakeId(uint32_t slot, uint16_t generation) { return ((GameObjectId)generation << 16) | slot; }

    std::vector<BuildingComponent> m_Buildings;
    std::vector<VoxelAnchorComponent> m_Anchors;
    std::vector<uint16_t> m_Generations; // Current generation per slot; never 0
    std::vector<uint8_t> m_Alive;
    std::vector<uint32_t> m_FreeSlots;   // Destroyed slots, reused before new ones
    uint32_t m_UsedSlots = 0;            // Slots handed out at least once
    uint32_t m_AliveCount = 0;
};
