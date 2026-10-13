#include "TestWorld.h"

#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Placement.h"
#include "World/Chunk.h"
#include "World/Islands.h"

#include <algorithm>
#include <cstdlib>

TestWorld& TestWorld::Get() {
    static TestWorld instance;
    return instance;
}

FoliageLibrary TestWorld::LoadFoliage() {
    FoliageLibrary foliage;
    foliage.Load(ASSET_DIRECTORY);
    return foliage;
}

TestWorld::TestWorld() {
    spawnColumn = terrain.FindSpawnColumn(SpawnSearchColumn()); // Same search start as the game
    LoadAround(spawnColumn, RADIUS_CHUNKS);
}

void TestWorld::LoadAround(glm::ivec2 column, int radiusChunks) {
    glm::ivec2 center(column.x >> 5, column.y >> 5);
    for (int cz = center.y - radiusChunks; cz <= center.y + radiusChunks; cz++) {
        for (int cx = center.x - radiusChunks; cx <= center.x + radiusChunks; cx++) {
            if (world.FindChunk(cx, 1, cz)) continue;
            Chunk* chunk = world.GetOrCreateChunk(cx, 1, cz);
            bareTerrain.GenerateChunk(cx, 1, cz, chunk->data);
        }
    }
}

bool TestWorld::FindCoastSpot(const PlacementContext& context, uint16_t type, glm::ivec2& minTile, uint8_t& rotation) {
    const BuildingType& building = BUILDING_TYPES[type];
    const int expectedLand = building.footprintWidth * (building.footprintDepth - building.dockRows);
    glm::ivec2 spawn(ColumnToTile(spawnColumn.x), ColumnToTile(spawnColumn.y));
    for (int ring = 0; ring < 100; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                glm::ivec2 tile = spawn + glm::ivec2(dx, dz);
                for (uint8_t r = 0; r < 4; r++) {
                    glm::ivec2 tiles = FootprintTiles(building, r);
                    int land = 0;
                    for (int z = 0; z < tiles.y; z++) {
                        for (int x = 0; x < tiles.x; x++) land += terrain.TileKindAt(tile.x + x, tile.y + z) == TerrainGenerator::TileKind::Land;
                    }
                    if (std::abs(land - expectedLand) > building.footprintWidth) continue;
                    LoadAround((tile + tiles / 2) * TILE_SIZE, 4);
                    if (ValidatePlacement(context, type, r, tile).error != PlacementError::None) continue;
                    minTile = tile;
                    rotation = r;
                    return true;
                }
            }
        }
    }
    return false;
}
