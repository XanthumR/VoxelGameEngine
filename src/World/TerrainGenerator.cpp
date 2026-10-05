#include "World/TerrainGenerator.h"

#include "World/BlockTypes.h"
#include "World/Chunk.h"
#include "World/VoxModel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

TerrainGenerator::TerrainGenerator(const VoxModel& trees) : m_Trees(trees) {
    m_Noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

    // Low-frequency mask deciding land vs ocean: islands a few hundred voxels across
    m_IslandNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_IslandNoise.SetFrequency(0.0025f);
    m_IslandNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_IslandNoise.SetFractalOctaves(3);
    m_IslandNoise.SetSeed(7);

    m_BiomeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_BiomeNoise.SetFrequency(0.005f); // Low frequency for large scale biomes

    m_CaveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_CaveNoise.SetFrequency(0.035f); // Tunnel-scale cave frequency
}

int TerrainGenerator::BiomeFromNoise(float biomeNoise) {
    if (biomeNoise < -0.2f) return CRYSTALLINE_PEAKS;
    if (biomeNoise > 0.2f) return VERDANT_CAVERNS;
    return CRUST;
}

float TerrainGenerator::BiomeNoise(int wx, int wz) {
    return m_BiomeNoise.GetNoise((float)wx, (float)wz);
}

// 0 = open ocean, 1 = island interior, smooth in between (the coast)
float TerrainGenerator::IslandMask(int wx, int wz) {
    float n = m_IslandNoise.GetNoise((float)wx, (float)wz);
    float t = glm::clamp(n / 0.3f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Blends the sea floor (~16 below sea level) into flat island ground ISLAND_HEIGHT above sea
// level; the blend band is the coast, which slopes down through the beach
int TerrainGenerator::TerrainHeightAt(int wx, int wz) {
    float heightSample = m_Noise.GetNoise((float)wx * HEIGHT_NOISE_SCALE, (float)wz * HEIGHT_NOISE_SCALE);
    float seaFloor = SEA_LEVEL - 16.0f + heightSample * 4.0f;
    float landTop = (float)(SEA_LEVEL + ISLAND_HEIGHT);
    return static_cast<int>(glm::mix(seaFloor, landTop, IslandMask(wx, wz)));
}

// The sea floor, and beaches along the coast (outer part of the island mask, up to a few
// voxels above the sea), are sand
bool TerrainGenerator::IsSandy(int wx, int wz, int terrainHeight) {
    if (terrainHeight <= SEA_LEVEL + 2) return true;
    return terrainHeight <= SEA_LEVEL + 6 && IslandMask(wx, wz) < 0.3f;
}

void TerrainGenerator::GenerateChunk(int cx, int cy, int cz, std::vector<uint8_t>& data) {
    data.assign(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, 0);
    int startX = cx * CHUNK_SIZE;
    int startY = cy * CHUNK_SIZE;
    int startZ = cz * CHUNK_SIZE;

    for (int z = 0; z < CHUNK_SIZE; z++) {
        int wz = startZ + z;
        for (int x = 0; x < CHUNK_SIZE; x++) {
            int wx = startX + x;

            int biome = BiomeFromNoise(BiomeNoise(wx, wz));
            int terrainHeight = TerrainHeightAt(wx, wz);
            bool sandy = IsSandy(wx, wz, terrainHeight);

            for (int y = 0; y < CHUNK_SIZE; y++) {
                int wy = startY + y;
                if (wy >= terrainHeight) {
                    // Ocean: everything above the ground up to sea level is water
                    if (wy <= SEA_LEVEL) data[LocalIndex(x, y, z)] = Block::WATER;
                    continue;
                }

                uint8_t blockID = Block::STONE;
                if (sandy && wy > terrainHeight - 4) {
                    blockID = Block::SAND; // Beaches and the sea floor
                }
                else if (wy == terrainHeight - 1) {
                    blockID = Block::GRASS;
                }
                else if (wy > terrainHeight - 4) {
                    blockID = Block::DIRT;
                }

                // Cave carving using abs(noise) for worm/tunnel shapes
                if (wy > 12 && wy < terrainHeight - 12) {
                    float caveValue = std::fabs(m_CaveNoise.GetNoise((float)wx, (float)wy, (float)wz)); // Tunnels near zero-crossings
                    if (biome == VERDANT_CAVERNS) {
                        // Wider tunnels with bioluminescent walls
                        if (caveValue < 0.06f) {
                            blockID = Block::AIR;
                        } else if (caveValue < 0.09f) {
                            blockID = Block::CAVERN_GLOW;
                        }
                    }
                    else if (biome == CRUST) {
                        if (caveValue < 0.04f) blockID = Block::AIR; // Narrow tunnels
                    }
                }

                data[LocalIndex(x, y, z)] = blockID;
            }
        }
    }

    // Trees on grass, rooted in this chunk or overhanging from a neighbour
    if (!m_Trees.IsEmpty()) StampTrees(startX, startY, startZ, data);

    // Grass tufts in their rest pose, after trees so they only fill air
    if (GRASS_TUFTS_ENABLED) StampGrass(startX, startY, startZ, data);
}

// Roots are scanned in a margin around the chunk so trees rooted in a neighbour stamp their
// overhanging part here too, keeping trees whole across chunk borders
void TerrainGenerator::StampTrees(int startX, int startY, int startZ, std::vector<uint8_t>& data) {
    const glm::ivec3 treeMin = m_Trees.min, treeMax = m_Trees.max;
    for (int rz = -treeMax.z; rz < CHUNK_SIZE - treeMin.z; rz++) {
        int wz = startZ + rz;
        for (int rx = -treeMax.x; rx < CHUNK_SIZE - treeMin.x; rx++) {
            int wx = startX + rx;

            if (BiomeNoise(wx, wz) < -0.2f) continue; // No trees in the Crystalline Peaks biome (open grassland)

            float treeNoise = m_Noise.GetNoise((float)wx * 15.0f, (float)wz * 15.0f);
            if (treeNoise <= 0.85f) continue;

            int rootY = TerrainHeightAt(wx, wz) - 1;
            if (IsSandy(wx, wz, rootY + 1)) continue; // No trees on beaches or underwater
            int localY = rootY - startY;
            if (localY + treeMax.y < 0 || localY + treeMin.y >= CHUNK_SIZE) continue;

            for (const VoxelOffset& offset : m_Trees.voxels) {
                int tx = rx + offset.x;
                int ty = localY + offset.y;
                int tz = rz + offset.z;
                if (tx >= 0 && tx < CHUNK_SIZE && ty >= 0 && ty < CHUNK_SIZE && tz >= 0 && tz < CHUNK_SIZE) {
                    data[LocalIndex(tx, ty, tz)] = (uint8_t)offset.blockType;
                }
            }
        }
    }
}

// Decided only by noise and a hash of the cell, so every chunk (and the main thread) agrees on
// where tufts are
bool TerrainGenerator::GrassTuftInCell(int gx, int gz, GrassTuft& tuft) {
    uint32_t h = GrassCellHash(gx, gz);
    if ((h & 0xFF) < 40) return false; // ~15% of cells stay bare, for natural gaps

    int wx = gx * GRASS_CELL + (int)((h >> 8) & (GRASS_CELL - 1));
    int wz = gz * GRASS_CELL + (int)((h >> 10) & (GRASS_CELL - 1));
    if (BiomeNoise(wx, wz) < -0.2f) return false; // No tufts in the Crystalline Peaks biome

    int terrainHeight = TerrainHeightAt(wx, wz);
    if (IsSandy(wx, wz, terrainHeight)) return false; // Beaches and the sea floor are sand, not grass

    tuft.root = glm::ivec3(wx, terrainHeight, wz);
    tuft.variant = (int)((h >> 12) & 3);
    tuft.phase = (float)((h >> 14) & 255) / 255.0f * 0.7f;
    return true;
}

void TerrainGenerator::StampGrass(int startX, int startY, int startZ, std::vector<uint8_t>& data) {
    // Blades sit up to 1 voxel off their root, so scan cells 1 voxel past the chunk border
    for (int gz = (startZ - 1) >> 2; gz <= (startZ + CHUNK_SIZE) >> 2; gz++) {
        for (int gx = (startX - 1) >> 2; gx <= (startX + CHUNK_SIZE) >> 2; gx++) {
            GrassTuft tuft;
            if (!GrassTuftInCell(gx, gz, tuft)) continue;
            if (tuft.root.y + GRASS_MAX_HEIGHT <= startY || tuft.root.y >= startY + CHUNK_SIZE) continue;

            for (int b = 0; b < GRASS_BLADES; b++) {
                const int* blade = GRASS_BLADE_SHAPES[tuft.variant][b];
                for (int level = 0; level < blade[2]; level++) {
                    int lx = tuft.root.x + blade[0] - startX;
                    int ly = tuft.root.y + level - startY;
                    int lz = tuft.root.z + blade[1] - startZ;
                    if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_SIZE) continue;
                    uint8_t& voxel = data[LocalIndex(lx, ly, lz)];
                    if (voxel == Block::AIR) voxel = Block::GRASS_TUFT;
                }
            }
        }
    }
}

glm::ivec2 TerrainGenerator::FindSpawnColumn(glm::ivec2 searchStart) {
    auto isLand = [this](int wx, int wz) {
        return TerrainHeightAt(wx, wz) >= SEA_LEVEL + ISLAND_HEIGHT;
    };
    const int STEP = 16, PROBE = 24, MAX_RINGS = 400;
    for (int ring = 0; ring <= MAX_RINGS; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue; // Ring edge only
                int x = searchStart.x + dx * STEP, z = searchStart.y + dz * STEP;
                if (isLand(x, z) && isLand(x + PROBE, z) && isLand(x - PROBE, z) && isLand(x, z + PROBE) && isLand(x, z - PROBE)) {
                    return glm::ivec2(x, z);
                }
            }
        }
    }
    return searchStart; // Nothing found; spawn over the ocean
}
