#pragma once

#include "Economy/ProductionChains.h"

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
    int count = 0;       // Trees or pasture tiles found
    int needed = 0;      // For full speed
    int factor = 1000;   // Productivity from the location, per mille
};

// Open sea within radiusTiles of the footprint (from the terrain, no voxels needed)
bool HasCoast(TerrainGenerator& terrain, glm::ivec2 minTile, glm::ivec2 tiles, int radiusTiles);

// Evaluates the rule of a chain for a footprint. counted (optional) receives the tiles that count:
// the free pasture tiles, or the tiles of the standing trees.
LocationReport EvaluateLocation(const ProductionChain& chain, glm::ivec2 minTile, glm::ivec2 tiles, IslandRegistry& islands,
    const OccupancyGrid& occupancy, const RoadNetwork& roads, TreeRegistry& trees, std::vector<glm::ivec2>* counted = nullptr);
