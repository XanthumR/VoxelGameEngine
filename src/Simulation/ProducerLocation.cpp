#include "Simulation/ProducerLocation.h"

#include "Simulation/BuildingTypes.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/RoadNetwork.h"
#include "Simulation/TreeRegistry.h"
#include "World/TerrainGenerator.h"
#include "World/WorldConstants.h"

#include <algorithm>

namespace {

constexpr int COAST_SAMPLE_STEP = 3; // Columns between coast samples (the sea is never that narrow)

} // namespace

bool HasCoast(TerrainGenerator& terrain, glm::ivec2 minTile, glm::ivec2 tiles, int radiusTiles) {
    glm::ivec2 minColumn = (minTile - radiusTiles) * TILE_SIZE;
    glm::ivec2 maxColumn = (minTile + tiles + radiusTiles) * TILE_SIZE - 1;
    for (int wz = minColumn.y; wz <= maxColumn.y; wz += COAST_SAMPLE_STEP) {
        for (int wx = minColumn.x; wx <= maxColumn.x; wx += COAST_SAMPLE_STEP) {
            if (terrain.TerrainHeightAt(wx, wz) <= SEA_LEVEL) return true;
        }
    }
    return false;
}

LocationReport EvaluateLocation(const ProductionChain& chain, glm::ivec2 minTile, glm::ivec2 tiles, IslandRegistry& islands,
    const OccupancyGrid& occupancy, const RoadNetwork& roads, TreeRegistry& trees, std::vector<glm::ivec2>* counted) {
    LocationReport report;
    if (counted) counted->clear();
    const int radius = chain.radius;
    report.needed = chain.fullSpeedCount;

    switch (chain.rule) {
    case LocationRule::None:
        break;

    case LocationRule::Coast:
        report.allowed = HasCoast(islands.Terrain(), minTile, tiles, radius);
        report.factor = report.allowed ? 1000 : 0;
        break;

    case LocationRule::Trees: {
        glm::ivec2 minColumn = (minTile - radius) * TILE_SIZE;
        glm::ivec2 maxColumn = (minTile + tiles + radius) * TILE_SIZE - 1;
        trees.ForEachStanding(minColumn, maxColumn, [&](glm::ivec2 root) {
            report.count++;
            if (counted) counted->push_back(glm::ivec2(ColumnToTile(root.x), ColumnToTile(root.y)));
        });
        report.factor = std::min(1000, report.count * 1000 / std::max(1, report.needed));
        break;
    }

    case LocationRule::Modules:
        // Where the modules may go: free island ground around the footprint, no building, no road
        for (int tz = minTile.y - radius; tz < minTile.y + tiles.y + radius; tz++) {
            for (int tx = minTile.x - radius; tx < minTile.x + tiles.x + radius; tx++) {
                glm::ivec2 tile(tx, tz);
                bool inFootprint = tx >= minTile.x && tz >= minTile.y && tx < minTile.x + tiles.x && tz < minTile.y + tiles.y;
                if (inFootprint || occupancy.At(tile) != INVALID_GAME_OBJECT || roads.IsRoad(tile)) continue;
                glm::ivec2 center = tile * TILE_SIZE + TILE_SIZE / 2;
                if (!islands.IsLandColumn(center.x, center.y)) continue;
                if (counted) counted->push_back(tile);
            }
        }
        report.factor = 0;
        break;
    }
    return report;
}

bool InModuleRange(const VoxelAnchorComponent& farm, int radius, glm::ivec2 moduleMinTile, glm::ivec2 moduleTiles) {
    glm::ivec2 farmMin(ColumnToTile(farm.origin.x), ColumnToTile(farm.origin.z));
    glm::ivec2 rangeMin = farmMin - radius;
    glm::ivec2 rangeMax = farmMin + farm.footprint / TILE_SIZE + radius; // Exclusive
    return glm::all(glm::greaterThanEqual(moduleMinTile, rangeMin)) && glm::all(glm::lessThanEqual(moduleMinTile + moduleTiles, rangeMax));
}

int CountModules(const GameObjectRegistry& objects, GameObjectId farm) {
    int radius = PRODUCTION_CHAINS[BUILDING_TYPES[objects.Building(farm).type].chain].radius;
    const VoxelAnchorComponent& farmAnchor = objects.Anchor(farm);
    int count = 0;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT || objects.Building(id).owner != farm) continue;
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::ivec2 minTile(ColumnToTile(anchor.origin.x), ColumnToTile(anchor.origin.z));
        if (InModuleRange(farmAnchor, radius, minTile, anchor.footprint / TILE_SIZE)) count++;
    }
    return count;
}

GameObjectId FindModuleFarm(const GameObjectRegistry& objects, uint16_t moduleType, IslandId island, glm::ivec2 minTile, glm::ivec2 tiles,
    GameObjectId preferred) {
    int farmType = BUILDING_TYPES[moduleType].moduleOf;
    if (farmType < 0) return INVALID_GAME_OBJECT;
    const ProductionChain& chain = PRODUCTION_CHAINS[BUILDING_TYPES[farmType].chain];
    auto fits = [&](GameObjectId farm) {
        return objects.IsAlive(farm) && objects.Building(farm).type == farmType && objects.Building(farm).island == island &&
               InModuleRange(objects.Anchor(farm), chain.radius, minTile, tiles) && CountModules(objects, farm) < chain.fullSpeedCount;
    };
    if (fits(preferred)) return preferred;
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id != INVALID_GAME_OBJECT && fits(id)) return id;
    }
    return INVALID_GAME_OBJECT;
}
