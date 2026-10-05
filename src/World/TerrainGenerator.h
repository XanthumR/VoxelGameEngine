#pragma once

#include "World/GrassTufts.h"
#include "ThirdParty/FastNoiseLite.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

struct VoxModel;

// Procedural world: flat islands in an ocean, with beaches, caves, trees and
// (optionally) grass tufts. Pure function of the coordinates, so any thread can generate any
// chunk and neighbouring chunks always agree. Each worker thread owns its own instance.
class TerrainGenerator {
public:
    explicit TerrainGenerator(const VoxModel& trees);

    // Fills data with CHUNK_SIZE^3 block IDs
    void GenerateChunk(int cx, int cy, int cz, std::vector<uint8_t>& data);

    // Height of the first air voxel above the ground (or sea floor) of a column
    int TerrainHeightAt(int wx, int wz);

    // The tuft in a GRASS_CELL x GRASS_CELL cell of columns, if it has one
    bool GrassTuftInCell(int gx, int gz, GrassTuft& tuft);

    // Nearest column (searched in rings from searchStart) that is solid land with land 72
    // voxels around it too, so the player does not start on a sliver of beach
    glm::ivec2 FindSpawnColumn(glm::ivec2 searchStart);

private:
    enum Biome { CRYSTALLINE_PEAKS = 1, VERDANT_CAVERNS = 2, CRUST = 3 };
    static int BiomeFromNoise(float biomeNoise);
    float BiomeNoise(int wx, int wz);
    float IslandMask(int wx, int wz);
    bool IsSandy(int wx, int wz, int terrainHeight);

    void StampTrees(int startX, int startY, int startZ, std::vector<uint8_t>& data);
    void StampGrass(int startX, int startY, int startZ, std::vector<uint8_t>& data);

    const VoxModel& m_Trees;
    FastNoiseLite m_Noise;       // Ground height, tree placement
    FastNoiseLite m_BiomeNoise;
    FastNoiseLite m_CaveNoise;
    FastNoiseLite m_IslandNoise; // Land vs ocean
    static constexpr float HEIGHT_NOISE_SCALE = 0.5f;
};
