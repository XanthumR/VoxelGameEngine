#pragma once

#include "World/GrassTufts.h"
#include "ThirdParty/FastNoiseLite.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

class FelledTrees;
struct VoxModel;

// Procedural world: flat islands in an ocean, with beaches, cliffs, caves, trees and
// (optionally) grass tufts. Pure function of the coordinates, so any thread can generate any
// chunk and neighbouring chunks always agree. Each worker thread owns its own instance.
//
// Islands are made of whole build tiles (TILE_SIZE x TILE_SIZE columns), like the blocky islands
// of Anno: every tile is
//  - Land:  flat grass at island height, all of it buildable
//  - Beach: a sea tile next to land, sand sloping from just under the grass into the water
//  - Cliff: a sea tile next to land where the grass ends in a rock edge over deep water
//  - Sea:   open water over a smooth sea floor
// Whether a coast tile is beach or cliff follows a low-frequency noise, so both come in stretches.
class TerrainGenerator {
public:
    enum class TileKind : uint8_t { Sea, Land, Beach, Cliff };

    explicit TerrainGenerator(const VoxModel& trees);

    TileKind TileKindAt(int tx, int tz);
    bool IsLandTile(int tx, int tz);

    // Fills data with CHUNK_SIZE^3 block IDs
    void GenerateChunk(int cx, int cy, int cz, std::vector<uint8_t>& data);

    // Height of the first air voxel above the ground (or sea floor) of a column
    int TerrainHeightAt(int wx, int wz);

    // The tuft in a GRASS_CELL x GRASS_CELL cell of columns, if it has one
    bool GrassTuftInCell(int gx, int gz, GrassTuft& tuft);

    // Nearest column (searched in rings from searchStart) that is solid land with land 72
    // voxels around it too, so the player does not start on a sliver of beach
    glm::ivec2 FindSpawnColumn(glm::ivec2 searchStart);

    // Trees. A tree stands on the column where the tree noise peaks within its clump, on grass
    // outside the open-grassland biome; the tree model is stamped with its root there. Trees in
    // the felled set (shared with the simulation) are left out of generated chunks.
    bool IsTreeRoot(int wx, int wz);
    int TreeRootY(int wx, int wz) { return TerrainHeightAt(wx, wz) - 1; } // The ground voxel under the trunk
    template <typename Function>
    void ForEachTreeRoot(glm::ivec2 minColumn, glm::ivec2 maxColumn, Function function) { // Inclusive bounds
        for (int wz = minColumn.y; wz <= maxColumn.y; wz++) {
            for (int wx = minColumn.x; wx <= maxColumn.x; wx++) {
                if (IsTreeRoot(wx, wz)) function(glm::ivec2(wx, wz));
            }
        }
    }
    void SetFelledTrees(const FelledTrees* felled) { m_Felled = felled; }
    const VoxModel& TreeModel() const { return m_Trees; }

    static constexpr int TREE_SPACING = 2; // A root is the highest tree noise within this many columns

private:
    float TreeNoise(int wx, int wz);
    enum Biome { CRYSTALLINE_PEAKS = 1, VERDANT_CAVERNS = 2, CRUST = 3 };
    static int BiomeFromNoise(float biomeNoise);
    float BiomeNoise(int wx, int wz);
    int SeaFloorHeight(int wx, int wz);
    int LandRing(int tx, int tz);
    int LandDistance(int wx, int wz, int radius);
    bool IsSandy(int wx, int wz);
    bool RawLand(int tx, int tz);     // The island noise at the tile's middle, before smoothing
    bool FacesCliff(int wx, int wz);  // A land column on the edge toward a cliff tile

    // Island noise per tile, cached (direct-mapped): a column's height needs the tiles around it
    struct TileSample {
        int32_t tx = 0, tz = 0;
        bool valid = false;
        bool land = false;
    };
    // What each tile is, and how far it is from land (filled in when first asked)
    struct TileInfo {
        int32_t tx = 0, tz = 0;
        bool valid = false;
        TileKind kind = TileKind::Sea;
        int8_t ring = -1;
    };
    TileInfo& Info(int tx, int tz);
    static constexpr size_t TILE_CACHE_SIZE = 4096;
    std::array<TileSample, TILE_CACHE_SIZE> m_TileCache;
    std::array<TileInfo, TILE_CACHE_SIZE> m_InfoCache;

    void StampTrees(int startX, int startY, int startZ, std::vector<uint8_t>& data);
    void StampGrass(int startX, int startY, int startZ, std::vector<uint8_t>& data);

    const VoxModel& m_Trees;
    const FelledTrees* m_Felled = nullptr;
    FastNoiseLite m_Noise;       // Ground height, tree placement
    FastNoiseLite m_BiomeNoise;
    FastNoiseLite m_CaveNoise;
    FastNoiseLite m_IslandNoise; // Land vs ocean
    FastNoiseLite m_CoastNoise;  // Beach vs cliff along the coast
    static constexpr float HEIGHT_NOISE_SCALE = 0.5f;
};
