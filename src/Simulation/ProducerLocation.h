#pragma once

#include "Economy/ProductionChains.h"
#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <vector>

class IslandRegistry;
class OccupancyGrid;
class RoadNetwork;
class TerrainGenerator;
class TreeRegistry;

// How good a spot is for a producer, by its chain's location rule (see ProductionChains.h)
struct LocationReport {
    bool allowed = true; // false: the spot breaks a required rule (no coast)
    int count = 0;       // Trees or modules found
    int needed = 0;      // For full speed
    int factor = 1000;   // Productivity from the location, per mille
};

// Open sea within radiusTiles of the footprint (from the terrain, no voxels needed)
bool HasCoast(TerrainGenerator& terrain, glm::ivec2 minTile, glm::ivec2 tiles, int radiusTiles);

// Evaluates the rule of a chain for a footprint. counted (optional) receives the tiles that count:
// the tiles of the standing trees, or for farms the free tiles their modules may go on (count stays
// 0: a farm's modules are counted by CountModules).
LocationReport EvaluateLocation(const ProductionChain& chain, glm::ivec2 minTile, glm::ivec2 tiles, IslandRegistry& islands,
    const OccupancyGrid& occupancy, const RoadNetwork& roads, TreeRegistry& trees, std::vector<glm::ivec2>* counted = nullptr);

// Farm modules (BuildingRole::Module). A module counts for the farm that owns it while it lies
// wholly within the farm chain's radius of the farm's footprint; a farm takes at most its chain's
// fullSpeedCount modules.
bool InModuleRange(const VoxelAnchorComponent& farm, int radius, glm::ivec2 moduleMinTile, glm::ivec2 moduleTiles);
int CountModules(const GameObjectRegistry& objects, GameObjectId farm);
// The farm a module of this type placed here would belong to: one on the island in range with room,
// preferred if it qualifies; INVALID when none
GameObjectId FindModuleFarm(const GameObjectRegistry& objects, uint16_t moduleType, IslandId island, glm::ivec2 minTile, glm::ivec2 tiles,
    GameObjectId preferred = INVALID_GAME_OBJECT);
