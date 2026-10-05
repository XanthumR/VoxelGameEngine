#include "Simulation/Logistics.h"

#include "Simulation/BuildingTypes.h"
#include "Simulation/RoadNetwork.h"

#include <cstdlib>

namespace {

const glm::ivec2 DIRECTIONS[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

void FootprintTilesOf(const VoxelAnchorComponent& anchor, glm::ivec2& minTile, glm::ivec2& tiles) {
    minTile = glm::ivec2(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
    tiles = anchor.footprint / TILE_SIZE;
}

} // namespace

LogisticsSystem::LogisticsSystem() {
    m_Queue.reserve(RoadNetwork::RESERVED_TILES);
}

bool LogisticsSystem::Update(GameObjectRegistry& objects, RoadNetwork& roads, uint32_t buildingsRevision) {
    if (roads.Revision() == m_LastRoadRevision && buildingsRevision == m_LastBuildingsRevision) return false;
    m_LastRoadRevision = roads.Revision();
    m_LastBuildingsRevision = buildingsRevision;
    Rebuild(objects, roads);
    return true;
}

void LogisticsSystem::Rebuild(GameObjectRegistry& objects, RoadNetwork& roads) {
    roads.ForEach([](glm::ivec2, RoadTile& road) {
        road.distance = RoadTile::UNREACHED;
        road.warehouse = INVALID_GAME_OBJECT;
    });

    // Seeds: the road tiles around every warehouse, in slot order so ties are deterministic
    m_Queue.clear();
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || objects.Building(id).type != BUILDING_WAREHOUSE) continue;
        glm::ivec2 minTile, tiles;
        FootprintTilesOf(objects.Anchor(id), minTile, tiles);
        ForEachTileAround(minTile, tiles, [&](glm::ivec2 tile) {
            RoadTile* road = roads.Find(tile);
            if (!road || road->distance <= 1) return;
            road->distance = 1;
            road->warehouse = id;
            m_Queue.push_back({ tile, 1 }); // Never more entries than road tiles: within the reserve
        });
    }

    // Breadth-first along the roads; the first warehouse to reach a tile owns it
    for (size_t head = 0; head < m_Queue.size(); head++) {
        QueueEntry entry = m_Queue[head];
        if (entry.distance >= WAREHOUSE_ROAD_RANGE) continue;
        GameObjectId warehouse = roads.Find(entry.tile)->warehouse;
        for (const glm::ivec2& direction : DIRECTIONS) {
            glm::ivec2 next = entry.tile + direction;
            RoadTile* road = roads.Find(next);
            if (!road || road->distance <= entry.distance + 1) continue;
            road->distance = (uint16_t)(entry.distance + 1);
            road->warehouse = warehouse;
            m_Queue.push_back({ next, road->distance });
        }
    }

    // Connect the buildings
    m_ConnectedBuildings = 0;
    m_TotalBuildings = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        LogisticsComponent& logistics = objects.Logistics(id);
        if (objects.Building(id).type == BUILDING_WAREHOUSE) {
            logistics.connected = true;
            logistics.warehouse = id;
            logistics.roadDistance = 0;
            continue;
        }
        glm::ivec2 minTile, tiles;
        FootprintTilesOf(objects.Anchor(id), minTile, tiles);
        FootprintConnection connection = ConnectionOf(roads, minTile, tiles);
        logistics.connected = connection.connected;
        logistics.warehouse = connection.warehouse;
        logistics.roadDistance = connection.roadDistance;
        m_TotalBuildings++;
        if (connection.connected) m_ConnectedBuildings++;
    }
    m_Revision++;
}

FootprintConnection LogisticsSystem::ConnectionOf(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles) {
    FootprintConnection best;
    ForEachTileAround(minTile, tiles, [&](glm::ivec2 tile) {
        const RoadTile* road = roads.Find(tile);
        if (!road || road->distance > WAREHOUSE_ROAD_RANGE || road->distance >= best.roadDistance) return;
        best.connected = true;
        best.warehouse = road->warehouse;
        best.roadDistance = road->distance;
    });
    return best;
}

void LogisticsSystem::PreviewReach(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, std::vector<glm::ivec2>& out) {
    out.clear();
    if (tiles.x > MAX_PREVIEW_FOOTPRINT || tiles.y > MAX_PREVIEW_FOOTPRINT) return;

    // A local grid around the footprint holds the distances, so the search allocates nothing
    const glm::ivec2 gridOrigin = minTile - glm::ivec2(WAREHOUSE_ROAD_RANGE + 1);
    auto cell = [&](glm::ivec2 tile) -> uint16_t* {
        glm::ivec2 local = tile - gridOrigin;
        if (local.x < 0 || local.y < 0 || local.x >= PREVIEW_SIDE || local.y >= PREVIEW_SIDE) return nullptr;
        return &m_PreviewDistance[(size_t)local.y * PREVIEW_SIDE + local.x];
    };
    m_PreviewDistance.fill(RoadTile::UNREACHED);

    m_Queue.clear();
    ForEachTileAround(minTile, tiles, [&](glm::ivec2 tile) {
        uint16_t* distance = cell(tile);
        if (!distance || *distance <= 1 || !roads.IsRoad(tile)) return;
        *distance = 1;
        m_Queue.push_back({ tile, 1 });
    });
    for (size_t head = 0; head < m_Queue.size(); head++) {
        QueueEntry entry = m_Queue[head];
        out.push_back(entry.tile);
        if (entry.distance >= WAREHOUSE_ROAD_RANGE) continue;
        for (const glm::ivec2& direction : DIRECTIONS) {
            glm::ivec2 next = entry.tile + direction;
            uint16_t* distance = cell(next);
            if (!distance || *distance <= entry.distance + 1 || !roads.IsRoad(next)) continue;
            *distance = (uint16_t)(entry.distance + 1);
            m_Queue.push_back({ next, *distance });
        }
    }
}
