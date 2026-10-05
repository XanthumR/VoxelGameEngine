#include "Gameplay/FishingBoats.h"

#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingModels.h"
#include "Simulation/Placement.h"
#include "World/TerrainGenerator.h"
#include "World/WorldConstants.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float SAILING_SECONDS = (FishingBoats::TRIP_SECONDS - FishingBoats::MOORED_SECONDS - FishingBoats::FISHING_SECONDS) / 2.0f;

// Figure direction index of a unit grid direction (0 +x, 1 -x, 2 +z, 3 -z)
int DirectionIndex(glm::ivec2 direction) {
    if (direction.x > 0) return 0;
    if (direction.x < 0) return 1;
    return direction.y > 0 ? 2 : 3;
}

} // namespace

FishingBoats::FishingBoats() {
    m_Boats.reserve(MAX_BOATS);
}

int FishingBoats::RouteLength(TerrainGenerator& terrain, glm::ivec2 start, glm::ivec2 direction, int maxDistance) {
    // The whole hull must be over water a voxel deep or more, so look ahead by the half length
    int length = 0;
    for (int step = 1; step <= maxDistance + BOAT_HALF_LENGTH; step++) {
        glm::ivec2 column = start + direction * step;
        if (terrain.TerrainHeightAt(column.x, column.y) > SEA_LEVEL - 1) break;
        length = step;
    }
    return std::clamp(length - BOAT_HALF_LENGTH, 0, maxDistance);
}

float FishingBoats::OffsetAt(float tripTime, int routeLength) {
    float t = std::fmod(std::max(tripTime, 0.0f), TRIP_SECONDS);
    if (t < MOORED_SECONDS) return 0.0f;
    t -= MOORED_SECONDS;
    if (t < SAILING_SECONDS) return routeLength * (t / SAILING_SECONDS);
    t -= SAILING_SECONDS;
    if (t < FISHING_SECONDS) return (float)routeLength;
    t -= FISHING_SECONDS;
    return routeLength * (1.0f - std::min(t / SAILING_SECONDS, 1.0f));
}

void FishingBoats::Update(float deltaTime, const GameObjectRegistry& objects, const BuildingModelLibrary& models, TerrainGenerator& terrain) {
    // Boats of demolished fisheries go
    for (size_t i = 0; i < m_Boats.size();) {
        if (!objects.IsAlive(m_Boats[i].fishery)) {
            m_Boats[i] = m_Boats.back();
            m_Boats.pop_back();
        } else {
            m_Boats[i].time = std::fmod(m_Boats[i].time + deltaTime, TRIP_SECONDS);
            i++;
        }
    }

    // New fisheries get a boat at their berth
    for (uint32_t slot = 0; slot < objects.SlotCount() && m_Boats.size() < (size_t)MAX_BOATS; slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingComponent& building = objects.Building(id);
        const BuildingModel* model = models.Model(building.type, building.variant);
        if (!model || model->boatBerths.empty()) continue;
        bool known = std::any_of(m_Boats.begin(), m_Boats.end(), [id](const Boat& boat) { return boat.fishery == id; });
        if (known) continue;

        const BuildingType& type = BUILDING_TYPES[building.type];
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::ivec2 size = FootprintColumns(type, building.rotation);
        const glm::ivec3& berth = model->boatBerths[0];
        // Out to sea is the model's back (+v), turned like the building
        glm::ivec2 direction = RotateToFootprint(0, 1, building.rotation, size) - RotateToFootprint(0, 0, building.rotation, size);
        glm::ivec2 column = glm::ivec2(anchor.origin.x, anchor.origin.z) + RotateToFootprint(berth.x, berth.y, building.rotation, size) +
                            direction * BOAT_HALF_LENGTH;

        Boat boat;
        boat.fishery = id;
        boat.start = glm::ivec3(column.x, BUILD_GROUND_Y - type.belowGround + berth.z, column.y);
        boat.direction = direction;
        boat.routeLength = RouteLength(terrain, column, direction, MAX_DISTANCE);
        boat.time = 0.0f;
        m_Boats.push_back(boat); // Within the reserve
    }
}

void FishingBoats::AppendFigures(std::vector<Figure>& out) const {
    for (const Boat& boat : m_Boats) {
        float offset = OffsetAt(boat.time, boat.routeLength);
        glm::ivec2 column = glm::ivec2(boat.start.x, boat.start.z) + glm::ivec2(glm::round(glm::vec2(boat.direction) * offset));
        float t = boat.time;
        bool returning = t >= MOORED_SECONDS + SAILING_SECONDS + FISHING_SECONDS;
        bool sailing = (t >= MOORED_SECONDS && t < MOORED_SECONDS + SAILING_SECONDS) || returning;
        int direction = DirectionIndex(returning ? -boat.direction : boat.direction);
        out.push_back({ glm::ivec4(column.x, boat.start.y, column.y, Figure::PackBoat(direction, sailing)) });
    }
}
