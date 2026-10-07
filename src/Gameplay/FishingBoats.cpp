#include "Gameplay/FishingBoats.h"

#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingModels.h"
#include "Simulation/Placement.h"
#include "World/BlockTypes.h"
#include "World/TerrainGenerator.h"
#include "World/WorldConstants.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float PI = 3.14159265f;
constexpr float SAILING_SECONDS = (FishingBoats::TRIP_SECONDS - FishingBoats::MOORED_SECONDS - FishingBoats::FISHING_SECONDS) / 2.0f;

// Model size: 7 wide, 13 tall, 19 long; the waterline is 2 voxels above the bottom
constexpr glm::ivec3 BOAT_SIZE(7, 13, 19);
constexpr float WATERLINE = 2.0f;

float Smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// From a toward b the short way round
float TurnToward(float a, float b, float t) {
    float delta = std::remainder(b - a, 2.0f * PI);
    return a + delta * t;
}

uint32_t Hash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

glm::vec2 Forward(float yaw) {
    return glm::vec2(std::sin(yaw), std::cos(yaw));
}

} // namespace

FishingBoats::FishingBoats() {
    m_Boats.reserve(MAX_BOATS);
}

VoxelObjectModel FishingBoats::BuildModel(bool sailSet) {
    VoxelObjectModel model;
    model.size = BOAT_SIZE;
    model.ids.assign((size_t)BOAT_SIZE.x * BOAT_SIZE.y * BOAT_SIZE.z, 0);
    auto put = [&](int s, int z, int y, uint8_t id) { // s: -3..3 across, z: 0 (stern) .. 18 (bow)
        int x = s + BOAT_SIZE.x / 2;
        if (x < 0 || x >= BOAT_SIZE.x || z < 0 || z >= BOAT_SIZE.z || y < 0 || y >= BOAT_SIZE.y) return;
        model.ids[(size_t)x + (size_t)BOAT_SIZE.x * ((size_t)z + (size_t)BOAT_SIZE.z * (size_t)y)] = id;
    };

    // Hull: half width along the length, narrowing to a pointed bow and a square stern
    auto halfWidth = [](int z) {
        if (z >= 18) return 0;
        if (z >= 16) return 1;
        if (z >= 14) return 2;
        return z == 0 ? 2 : 3;
    };
    for (int z = 0; z < BOAT_SIZE.z; z++) {
        int w = halfWidth(z);
        for (int s = -w; s <= w; s++) {
            if (std::abs(s) <= std::max(w - 2, 0)) put(s, z, 0, Block::TIMBER_DARK); // Keel
            if (std::abs(s) <= std::max(w - 1, 0)) put(s, z, 1, std::abs(s) == std::max(w - 1, 0) ? Block::SHUTTER_BLUE : Block::TIMBER_DARK);
            put(s, z, 2, std::abs(s) == w ? Block::BOAT_WOOD : Block::TIMBER_LIGHT); // Sides and the deck
            if (std::abs(s) == w) put(s, z, 3, Block::BOAT_WOOD);                     // Gunwale
        }
    }
    put(0, 18, 3, Block::BOAT_WOOD); // Bow post
    put(0, 18, 4, Block::BOAT_WOOD);

    // Wheelhouse at the stern with a red roof and a window forward
    for (int z = 2; z <= 4; z++) {
        for (int s = -1; s <= 1; s++) {
            for (int y = 3; y <= 5; y++) put(s, z, y, Block::TIMBER_LIGHT);
            put(s, z, 6, Block::ROOF_TILE_RED);
        }
    }
    put(0, 4, 4, Block::WINDOW_GLASS);

    // Catch on deck: crates and a basket of fish by the bow
    put(-2, 13, 3, Block::CRATE);
    put(-1, 13, 3, Block::CRATE);
    put(-2, 12, 3, Block::CRATE);
    put(1, 13, 3, Block::FISH_SILVER);
    put(2, 12, 3, Block::FISH_SILVER);

    // Mast and yard; the sail hangs behind the mast when set, furled on the yard otherwise
    for (int y = 3; y <= 12; y++) put(0, 10, y, Block::TIMBER_DARK);
    for (int s = -3; s <= 3; s++) put(s, 10, 12, Block::TIMBER_LIGHT);
    if (sailSet) {
        for (int y = 5; y <= 11; y++) {
            for (int s = -3; s <= 3; s++) put(s, 9, y, Block::AWNING_WHITE);
        }
    } else {
        for (int s = -3; s <= 3; s++) put(s, 9, 11, Block::AWNING_WHITE);
    }
    return model;
}

int FishingBoats::RouteLength(TerrainGenerator& terrain, glm::vec2 start, glm::vec2 direction, int maxDistance) {
    // The whole hull must be over water a voxel deep or more, so look ahead by the half length
    int length = 0;
    for (int step = 1; step <= maxDistance + BOAT_HALF_LENGTH; step++) {
        glm::ivec2 column = glm::ivec2(glm::floor(start + direction * (float)step + 0.5f));
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

float FishingBoats::HeadingAt(float tripTime, float mooredHeading, float tripHeading) {
    float t = std::fmod(std::max(tripTime, 0.0f), TRIP_SECONDS);
    if (t < MOORED_SECONDS) return mooredHeading;
    t -= MOORED_SECONDS;
    if (t < SAILING_SECONDS) return TurnToward(mooredHeading, tripHeading, Smooth(t / TURN_SECONDS));
    t -= SAILING_SECONDS;
    if (t < FISHING_SECONDS) return tripHeading + PI * Smooth(t / FISHING_SECONDS); // Turning around
    return tripHeading + PI;
}

void FishingBoats::StartTrip(Boat& boat, TerrainGenerator& terrain) {
    uint32_t h = Hash(boat.fishery * 2654435761u + boat.trip * 40503u);
    float angle = (float)((int)(h % (2 * MAX_TRIP_ANGLE + 1)) - MAX_TRIP_ANGLE) * PI / 180.0f;
    boat.tripHeading = boat.dockHeading + angle;
    boat.routeLength = RouteLength(terrain, boat.start, Forward(boat.tripHeading), MAX_DISTANCE);
}

void FishingBoats::Update(float deltaTime, const GameObjectRegistry& objects, const BuildingModelLibrary& models, TerrainGenerator& terrain) {

    // Boats of demolished fisheries go
    for (size_t i = 0; i < m_Boats.size();) {
        if (!objects.IsAlive(m_Boats[i].fishery) || objects.Anchor(m_Boats[i].fishery).origin != m_Boats[i].origin ||
            objects.Building(m_Boats[i].fishery).rotation != m_Boats[i].rotation) {
            m_Boats[i] = m_Boats.back();
            m_Boats.pop_back();
            continue;
        }
        // At the fishery's pace while it works; home at full speed and then moored when it does not
        Boat& boat = m_Boats[i];
        const ProductionComponent& production = objects.Production(boat.fishery);
        bool working = production.status == ProducerStatus::Working;
        float pace = working ? production.productivity / 1000.0f : 1.0f;
        if (working || boat.time > 0.0f) {
            float time = boat.time + deltaTime * pace;
            if (time >= TRIP_SECONDS) {
                // Back at the berth, bow toward land; the next trip goes another way
                boat.mooredHeading = boat.tripHeading + PI;
                boat.trip++;
                StartTrip(boat, terrain);
            }
            boat.time = (!working && time >= TRIP_SECONDS) ? 0.0f : std::fmod(time, TRIP_SECONDS);
        }
        i++;
    }

    // New fisheries get a boat at their berth
    for (uint32_t slot = 0; slot < objects.SlotCount() && m_Boats.size() < (size_t)MAX_BOATS; slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        const BuildingComponent& building = objects.Building(id);
        const BuildingModel* model = models.Model(building.type, building.variant);
        if (!model || model->boatBerths.empty() || BUILDING_TYPES[building.type].role != BuildingRole::Producer) continue; // Fisheries, not harbors
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
        boat.origin = anchor.origin;
        boat.rotation = building.rotation;
        boat.start = glm::vec2(column) + 0.5f;
        boat.dockHeading = std::atan2((float)direction.x, (float)direction.y);
        boat.mooredHeading = boat.dockHeading;
        boat.trip = 0;
        boat.time = 0.0f;
        StartTrip(boat, terrain);
        m_Boats.push_back(boat); // Within the reserve
    }
}

void FishingBoats::AppendObjects(std::vector<VoxelObject>& out) const {
    for (const Boat& boat : m_Boats) {
        float t = boat.time;
        bool sailing = t >= MOORED_SECONDS && !(t >= MOORED_SECONDS + SAILING_SECONDS && t < MOORED_SECONDS + SAILING_SECONDS + FISHING_SECONDS);
        glm::vec2 at = boat.start + Forward(boat.tripHeading) * OffsetAt(t, boat.routeLength);

        // It rides the waves (the GPU sets its height, pitch and roll from the ocean)
        VoxelObject object;
        object.model = sailing ? m_SailingModel : m_MooredModel;
        object.position = glm::vec3(at.x, SEA_LEVEL + 1.0f - WATERLINE, at.y);
        object.yaw = HeadingAt(t, boat.mooredHeading, boat.tripHeading);
        object.waterline = WATERLINE;
        out.push_back(object); // Within the caller's reserve
    }
}
