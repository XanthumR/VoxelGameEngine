#include "Simulation/Logistics.h"

#include "Simulation/BuildingTypes.h"

#include <cstdlib>

namespace {

const glm::ivec2 DIRECTIONS[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
constexpr int WAREHOUSE_REACH = -1; // Reach kind of warehouses; services use their ServiceType

// A road tile's distance and source for one kind of reach (Tile: RoadTile or const RoadTile)
template <typename Tile>
auto& DistanceOf(Tile& road, int kind) {
    return kind == WAREHOUSE_REACH ? road.distance : road.serviceDistance[kind];
}
template <typename Tile>
auto& SourceOf(Tile& road, int kind) {
    return kind == WAREHOUSE_REACH ? road.warehouse : road.service[kind];
}

void FootprintTilesOf(const VoxelAnchorComponent& anchor, glm::ivec2& minTile, glm::ivec2& tiles) {
    minTile = glm::ivec2(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
    tiles = anchor.footprint / TILE_SIZE;
}

// The nearest reached road tile (within range) touching a footprint, for one kind of reach
FootprintConnection BestAround(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, int range, int kind) {
    FootprintConnection best;
    ForEachTileAround(minTile, tiles, [&](glm::ivec2 tile) {
        const RoadTile* road = roads.Find(tile);
        if (!road) return;
        uint16_t distance = DistanceOf(*road, kind);
        if (distance > range || distance >= best.roadDistance) return;
        best.connected = true;
        best.source = SourceOf(*road, kind);
        best.roadDistance = distance;
    });
    return best;
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

void LogisticsSystem::Seed(RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, GameObjectId source, int kind) {
    ForEachTileAround(minTile, tiles, [&](glm::ivec2 tile) {
        RoadTile* road = roads.Find(tile);
        if (!road || DistanceOf(*road, kind) <= 1) return;
        DistanceOf(*road, kind) = 1;
        SourceOf(*road, kind) = source;
        m_Queue.push_back({ tile, 1 }); // Never more entries than road tiles: within the reserve
    });
}

void LogisticsSystem::Spread(RoadNetwork& roads, int range, int kind) {
    for (size_t head = 0; head < m_Queue.size(); head++) {
        QueueEntry entry = m_Queue[head];
        if (entry.distance >= range) continue;
        GameObjectId source = SourceOf(*roads.Find(entry.tile), kind);
        for (const glm::ivec2& direction : DIRECTIONS) {
            glm::ivec2 next = entry.tile + direction;
            RoadTile* road = roads.Find(next);
            if (!road || DistanceOf(*road, kind) <= entry.distance + 1) continue;
            DistanceOf(*road, kind) = (uint16_t)(entry.distance + 1);
            SourceOf(*road, kind) = source;
            m_Queue.push_back({ next, (uint16_t)(entry.distance + 1) });
        }
    }
}

void LogisticsSystem::Rebuild(GameObjectRegistry& objects, RoadNetwork& roads) {
    roads.ForEach([](glm::ivec2, RoadTile& road) { road = RoadTile(); });

    // 1. Warehouse reach, seeded in slot order so ties are deterministic
    m_Queue.clear();
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || BUILDING_TYPES[objects.Building(id).type].role != BuildingRole::Storage) continue;
        glm::ivec2 minTile, tiles;
        FootprintTilesOf(objects.Anchor(id), minTile, tiles);
        Seed(roads, minTile, tiles, id, WAREHOUSE_REACH);
    }
    Spread(roads, WAREHOUSE_ROAD_RANGE, WAREHOUSE_REACH);

    // 2. Warehouse connection of every building
    m_ConnectedBuildings = 0;
    m_TotalBuildings = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        LogisticsComponent& logistics = objects.Logistics(id);
        logistics = LogisticsComponent();
        if (BUILDING_TYPES[objects.Building(id).type].role == BuildingRole::Storage) {
            logistics.connected = true;
            logistics.warehouse = id;
            logistics.roadDistance = 0;
            continue;
        }
        glm::ivec2 minTile, tiles;
        FootprintTilesOf(objects.Anchor(id), minTile, tiles);
        FootprintConnection connection = ConnectionOf(roads, minTile, tiles);
        logistics.connected = connection.connected;
        logistics.warehouse = connection.source;
        logistics.roadDistance = connection.roadDistance;
        m_TotalBuildings++;
        if (connection.connected) m_ConnectedBuildings++;
    }

    // 3. Reach of the connected service buildings, one type at a time
    for (int kind = 0; kind < SERVICE_COUNT; kind++) {
        m_Queue.clear();
        for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
            GameObjectId id = objects.IdAtSlot(slot);
            if (id == INVALID_GAME_OBJECT || BUILDING_TYPES[objects.Building(id).type].service != (ServiceType)kind) continue;
            if (!objects.Logistics(id).connected) continue;
            glm::ivec2 minTile, tiles;
            FootprintTilesOf(objects.Anchor(id), minTile, tiles);
            Seed(roads, minTile, tiles, id, kind);
        }
        Spread(roads, SERVICE_ROAD_RANGE[kind], kind);
    }

    // 4. Which buildings each service reaches
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        glm::ivec2 minTile, tiles;
        FootprintTilesOf(objects.Anchor(id), minTile, tiles);
        LogisticsComponent& logistics = objects.Logistics(id);
        for (int kind = 0; kind < SERVICE_COUNT; kind++) {
            logistics.services[kind] = BestAround(roads, minTile, tiles, SERVICE_ROAD_RANGE[kind], kind).source;
        }
    }
    m_Revision++;
}

FootprintConnection LogisticsSystem::ConnectionOf(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles) {
    return BestAround(roads, minTile, tiles, WAREHOUSE_ROAD_RANGE, WAREHOUSE_REACH);
}

FootprintConnection LogisticsSystem::ServiceConnectionOf(const RoadNetwork& roads, ServiceType service, glm::ivec2 minTile, glm::ivec2 tiles) {
    return BestAround(roads, minTile, tiles, SERVICE_ROAD_RANGE[(size_t)service], (int)service);
}

void LogisticsSystem::PreviewReach(const RoadNetwork& roads, glm::ivec2 minTile, glm::ivec2 tiles, int range, std::vector<glm::ivec2>& out) {
    out.clear();
    if (tiles.x > MAX_PREVIEW_FOOTPRINT || tiles.y > MAX_PREVIEW_FOOTPRINT || range > WAREHOUSE_ROAD_RANGE) return;

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
        if (entry.distance >= range) continue;
        for (const glm::ivec2& direction : DIRECTIONS) {
            glm::ivec2 next = entry.tile + direction;
            uint16_t* distance = cell(next);
            if (!distance || *distance <= entry.distance + 1 || !roads.IsRoad(next)) continue;
            *distance = (uint16_t)(entry.distance + 1);
            m_Queue.push_back({ next, *distance });
        }
    }
}
