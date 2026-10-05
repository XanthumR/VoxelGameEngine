#include "Gameplay/Smoke.h"

#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingModels.h"
#include "Simulation/Placement.h"

#include <algorithm>
#include <cmath>

namespace {

const glm::vec2 WIND_DIRECTION = glm::normalize(glm::vec2(1.0f, 0.35f)); // The same wind as the grass (GrassAnimator)

} // namespace

SmokeSystem::SmokeSystem() {
    m_Puffs.reserve(MAX_PUFFS);
    m_Timers.assign((size_t)GameObjectRegistry::MAX_OBJECTS * MAX_EMITTERS_PER_BUILDING, -1.0f);
}

uint32_t SmokeSystem::Random() {
    m_RandomState ^= m_RandomState << 13;
    m_RandomState ^= m_RandomState >> 17;
    m_RandomState ^= m_RandomState << 5;
    return m_RandomState;
}

bool SmokeSystem::InUse(const GameObjectRegistry& objects, GameObjectId id) {
    switch (BUILDING_TYPES[objects.Building(id).type].role) {
    case BuildingRole::Residence: return objects.Residence(id).residents > 0;
    case BuildingRole::Producer: return objects.Production(id).status == ProducerStatus::Working;
    default: return false;
    }
}

void SmokeSystem::Update(float deltaTime, const GameObjectRegistry& objects, const BuildingModelLibrary& models, glm::vec2 focusColumn, float maxDistance) {
    // Age, rise and drift; old puffs go
    for (size_t i = 0; i < m_Puffs.size();) {
        Puff& puff = m_Puffs[i];
        puff.age += deltaTime;
        if (puff.age >= LIFETIME) {
            puff = m_Puffs.back();
            m_Puffs.pop_back();
            continue;
        }
        puff.position.y += RISE_SPEED * deltaTime;
        puff.position.x += WIND_DIRECTION.x * DRIFT_SPEED * deltaTime;
        puff.position.z += WIND_DIRECTION.y * DRIFT_SPEED * deltaTime;
        i++;
    }

    // New puffs from the chimneys of buildings in use near the focus
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingComponent& building = objects.Building(id);
        const BuildingModel* model = models.Model(building.type, building.variant);
        if (!model || model->smokeEmitters.empty()) continue;
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::vec2 center = glm::vec2(anchor.origin.x, anchor.origin.z) + glm::vec2(anchor.footprint) * 0.5f;
        bool active = InUse(objects, id) && glm::length(center - focusColumn) <= maxDistance;

        const BuildingType& type = BUILDING_TYPES[building.type];
        float interval = building.type == BUILDING_SLAUGHTERHOUSE ? PUFF_INTERVAL * 0.5f : PUFF_INTERVAL; // The smokehouse
        glm::ivec2 size = FootprintColumns(type, building.rotation);
        int emitters = std::min((int)model->smokeEmitters.size(), MAX_EMITTERS_PER_BUILDING);
        for (int e = 0; e < emitters; e++) {
            float& timer = m_Timers[(size_t)slot * MAX_EMITTERS_PER_BUILDING + e];
            if (!active) {
                timer = -1.0f;
                continue;
            }
            if (timer < 0.0f) timer = interval * (float)(Random() % 1000) / 1000.0f; // Chimneys out of step
            timer -= deltaTime;
            if (timer > 0.0f) continue;
            timer += interval;
            if (m_Puffs.size() >= (size_t)MAX_PUFFS) continue;

            const glm::ivec3& marker = model->smokeEmitters[e];
            glm::ivec2 column = RotateToFootprint(marker.x, marker.y, building.rotation, size);
            glm::vec3 position((float)(anchor.origin.x + column.x), (float)(BUILD_GROUND_Y - type.belowGround + marker.z + 1),
                (float)(anchor.origin.z + column.y));
            m_Puffs.push_back({ position, 0.0f, (uint8_t)(Random() & 255) }); // Within the reserve
        }
    }
}

void SmokeSystem::AppendFigures(std::vector<Figure>& out) const {
    for (const Puff& puff : m_Puffs) {
        float life = puff.age / LIFETIME;
        int size = life < 0.15f ? 1 : life < 0.4f ? 2 : 3;
        int density = 15 - (int)(12.0f * life); // Thins out as it ages
        glm::ivec3 voxel = glm::ivec3(glm::floor(puff.position + 0.5f));
        out.push_back({ glm::ivec4(voxel, Figure::PackPuff(size, density, puff.seed)) });
    }
}
