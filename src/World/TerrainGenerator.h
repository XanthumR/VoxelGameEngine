#pragma once

#include "World/GrassTufts.h"
#include "ThirdParty/FastNoiseLite.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

class FelledTrees;
class FoliageLibrary;
struct VoxModel;

// Procedural world: flat islands in an ocean, with beaches, cliffs, caves, trees and
// (optionally) grass tufts. Pure function of the coordinates, so any thread can generate any
// chunk and neighbouring chunks always agree. Each worker thread owns its own instance.
//
// Islands are made of whole build tiles (TILE_SIZE x TILE_SIZE columns), like the blocky islands
// of Anno, laid out per session by size class (World/Islands.h): every tile is
//  - Land:  flat grass at island height, all of it buildable
//  - Beach: a sea tile next to land, sand sloping from just under the grass into the water
//  - Cliff: a sea tile next to land where the grass ends in a rock edge over deep water
//  - Sea:   open water over a smooth sea floor
// Whether a coast tile is beach or cliff follows a low-frequency noise, so both come in stretches.
class TerrainGenerator {
public:
    enum class TileKind : uint8_t { Sea, Land, Beach, Cliff };

    // foliage: the models of World/Foliage.h (an empty library grows nothing)
    explicit TerrainGenerator(const FoliageLibrary& foliage);

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

    // Foliage (World/Foliage.h): forests and meadows, as in Anno. A low-frequency forest noise
    // makes forests and open meadows. Trees stand one per TREE_CELL x TREE_CELL cell of columns at
    // a spot hashed from the cell, nearly always in a forest and seldom in a meadow; the ground
    // cover (bushes, rocks, logs, flowers) one per DECOR_CELL cell, its kind by the forest noise.
    // Only on land with land round the root. The model is stamped with its root on the ground
    // voxel. Foliage in the felled set (shared with the simulation) is left out of generated chunks.
    int FoliageAt(int wx, int wz); // The FoliageLibrary model rooted at this column, or -1
    int TreeRootY(int wx, int wz) { return TerrainHeightAt(wx, wz) - 1; } // The ground voxel under the trunk
    template <typename Function>
    void ForEachFoliageRoot(glm::ivec2 minColumn, glm::ivec2 maxColumn, Function function) { // function(root, model); inclusive bounds
        ForEachCellRoot(minColumn, maxColumn, TREE_CELL, [&](int cx, int cz, glm::ivec2& root) { return TreeInCell(cx, cz, root); }, function);
        ForEachCellRoot(minColumn, maxColumn, DECOR_CELL, [&](int cx, int cz, glm::ivec2& root) { return DecorInCell(cx, cz, root); }, function);
    }
    void SetFelledTrees(const FelledTrees* felled) { m_Felled = felled; }
    const VoxModel& FoliageModel(int model) const;
    int FoliageReach() const; // Columns the widest model reaches from its root along x or z

    static constexpr int TREE_CELL = 9, DECOR_CELL = 6;

private:
    static int FloorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
    template <typename InCell, typename Function>
    void ForEachCellRoot(glm::ivec2 minColumn, glm::ivec2 maxColumn, int cell, InCell inCell, Function function) {
        for (int cz = FloorDiv(minColumn.y, cell); cz <= FloorDiv(maxColumn.y, cell); cz++) {
            for (int cx = FloorDiv(minColumn.x, cell); cx <= FloorDiv(maxColumn.x, cell); cx++) {
                glm::ivec2 root;
                int model = inCell(cx, cz, root);
                if (model >= 0 && root.x >= minColumn.x && root.y >= minColumn.y && root.x <= maxColumn.x && root.y <= maxColumn.y) function(root, model);
            }
        }
    }
    int TreeInCell(int cx, int cz, glm::ivec2& root);  // The cell's model, or -1; root: its spot
    int DecorInCell(int cx, int cz, glm::ivec2& root);
    int PickKind(bool trees, glm::ivec2 root, float forest, uint32_t hash); // A model for the spot, or -1
    float ForestNoise(int wx, int wz); // 0 open meadow .. 1 deep forest
    enum Biome { CRYSTALLINE_PEAKS = 1, VERDANT_CAVERNS = 2, CRUST = 3 };
    static int BiomeFromNoise(float biomeNoise);
    float BiomeNoise(int wx, int wz);
    int SeaFloorHeight(int wx, int wz);
    int LandRing(int tx, int tz);
    int LandDistance(int wx, int wz, int radius);
    bool IsSandy(int wx, int wz);
    bool FacesCliff(int wx, int wz);  // A land column on the edge toward a cliff tile

    // What each tile is, and how far it is from land (filled in when first asked)
    struct TileInfo {
        int32_t tx = 0, tz = 0;
        bool valid = false;
        TileKind kind = TileKind::Sea;
        int8_t ring = -1;
    };
    TileInfo& Info(int tx, int tz);
    static constexpr size_t TILE_CACHE_SIZE = 4096;
    std::array<TileInfo, TILE_CACHE_SIZE> m_InfoCache;

    void StampTrees(int startX, int startY, int startZ, std::vector<uint8_t>& data);
    void StampGrass(int startX, int startY, int startZ, std::vector<uint8_t>& data);

    const FoliageLibrary& m_Foliage;
    const FelledTrees* m_Felled = nullptr;
    FastNoiseLite m_Noise;       // Ground height
    FastNoiseLite m_ForestNoise; // Forests and meadows
    FastNoiseLite m_BiomeNoise;
    FastNoiseLite m_CaveNoise;
    FastNoiseLite m_CoastNoise;  // Beach vs cliff along the coast
    static constexpr float HEIGHT_NOISE_SCALE = 0.5f;
};
