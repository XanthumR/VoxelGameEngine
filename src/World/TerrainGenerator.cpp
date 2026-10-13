#include "World/TerrainGenerator.h"

#include "World/BlockTypes.h"
#include "World/Chunk.h"
#include "World/FelledTrees.h"
#include "World/Foliage.h"
#include "World/Islands.h"
#include "World/Sessions.h"
#include "World/VoxModel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

TerrainGenerator::TerrainGenerator(const FoliageLibrary& foliage) : m_Foliage(foliage) {
    m_Noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

    // Forests and meadows a few dozen tiles across
    m_ForestNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_ForestNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_ForestNoise.SetFractalOctaves(2);
    m_ForestNoise.SetFrequency(0.0065f);
    m_ForestNoise.SetSeed(21);

    // Stretches of cliff a few dozen tiles long between beaches
    m_CoastNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_CoastNoise.SetFrequency(0.0025f);
    m_CoastNoise.SetSeed(11);

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

namespace {

constexpr float CLIFF_NOISE = 0.25f;  // Coast noise above which a coast tile is a cliff
constexpr float TROPICAL_CLIFF_NOISE = 0.55f; // Fewer cliffs in the tropics: mostly beaches
constexpr int BEACH_TOP = SEA_LEVEL + 3;    // First air over the sand next to the grass (one below it)
constexpr int BEACH_BOTTOM = SEA_LEVEL - 2; // ...and at the far side of the beach tile, under water
constexpr int CLIFF_DEPTH = 6;        // The sea at the foot of a cliff (and one tile out) is at least this deep
constexpr int GROUND_TOP = SEA_LEVEL + ISLAND_HEIGHT; // First air over the highest ground (island grass; water is lower)
constexpr int SHELF_TILES = 5;        // How far out the sea floor still slopes down from the coast
constexpr float SHELF_SLOPE = 0.35f;  // Voxels down per column beyond the beach

uint32_t TileHash(int tx, int tz) {
    return ((uint32_t)tx * 73856093u) ^ ((uint32_t)tz * 19349663u);
}

} // namespace

// The islands are laid out per session (World/Islands.h)
bool TerrainGenerator::IsLandTile(int tx, int tz) {
    return IsIslandTile(glm::ivec2(tx, tz));
}

TerrainGenerator::TileInfo& TerrainGenerator::Info(int tx, int tz) {
    TileInfo& info = m_InfoCache[(TileHash(tx, tz) * 2654435761u >> 20) & (TILE_CACHE_SIZE - 1)];
    if (info.valid && info.tx == tx && info.tz == tz) return info;

    TileKind kind = TileKind::Sea;
    if (IsLandTile(tx, tz)) {
        kind = TileKind::Land;
    } else {
        bool coast = false;
        for (int dz = -1; dz <= 1 && !coast; dz++) {
            for (int dx = -1; dx <= 1 && !coast; dx++) coast = (dx != 0 || dz != 0) && IsLandTile(tx + dx, tz + dz);
        }
        if (coast) {
            float middle = TILE_SIZE * 0.5f;
            float noise = m_CoastNoise.GetNoise((float)tx * TILE_SIZE + middle, (float)tz * TILE_SIZE + middle);
            int session = SessionAtTile(glm::ivec2(tx, tz));
            float cliff = session >= 0 && SESSIONS[session].tropical ? TROPICAL_CLIFF_NOISE : CLIFF_NOISE;
            kind = noise > cliff ? TileKind::Cliff : TileKind::Beach;
        }
    }
    info = { tx, tz, true, kind, -1 };
    return info;
}

TerrainGenerator::TileKind TerrainGenerator::TileKindAt(int tx, int tz) {
    return Info(tx, tz).kind;
}

// Tiles (square rings) to the nearest land tile, up to SHELF_TILES + 1 for "far"
int TerrainGenerator::LandRing(int tx, int tz) {
    int ring = Info(tx, tz).ring;
    if (ring >= 0) return ring;
    ring = SHELF_TILES + 1;
    for (int r = 0; r <= SHELF_TILES && ring > SHELF_TILES; r++) {
        for (int dz = -r; dz <= r && ring > SHELF_TILES; dz++) {
            for (int dx = -r; dx <= r; dx++) {
                if (std::max(std::abs(dx), std::abs(dz)) == r && IsLandTile(tx + dx, tz + dz)) {
                    ring = r;
                    break;
                }
            }
        }
    }
    Info(tx, tz).ring = (int8_t)ring; // Looked up again: the search may have reused the cache slot
    return ring;
}

// Columns (square distance) from a column to the nearest land tile within radius tiles of its tile
int TerrainGenerator::LandDistance(int wx, int wz, int radius) {
    int tx = ColumnToTile(wx), tz = ColumnToTile(wz);
    int distance = (radius + 1) * TILE_SIZE;
    for (int dz = -radius; dz <= radius; dz++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (!IsLandTile(tx + dx, tz + dz)) continue;
            int minX = (tx + dx) * TILE_SIZE, minZ = (tz + dz) * TILE_SIZE;
            int awayX = std::max({ 0, minX - wx, wx - (minX + TILE_SIZE - 1) });
            int awayZ = std::max({ 0, minZ - wz, wz - (minZ + TILE_SIZE - 1) });
            distance = std::min(distance, std::max(awayX, awayZ));
        }
    }
    return distance;
}

// The sea floor: past the beach it slopes down with the distance to land until it meets the deep
// floor (~16 below sea level); in front of cliffs it is deep at once
int TerrainGenerator::SeaFloorHeight(int wx, int wz) {
    int tx = ColumnToTile(wx), tz = ColumnToTile(wz);
    float heightSample = m_Noise.GetNoise((float)wx * HEIGHT_NOISE_SCALE, (float)wz * HEIGHT_NOISE_SCALE);
    float deep = SEA_LEVEL - 16.0f + heightSample * 4.0f;
    int ring = LandRing(tx, tz);
    if (ring > SHELF_TILES) return (int)deep;

    int distance = LandDistance(wx, wz, ring + 1);
    float shelf = BEACH_BOTTOM - std::max(0, distance - TILE_SIZE) * SHELF_SLOPE;
    int height = (int)std::max(deep, std::min(shelf, (float)BEACH_BOTTOM));
    if (ring <= 2) {
        for (int dz = -1; dz <= 1; dz++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (TileKindAt(tx + dx, tz + dz) == TileKind::Cliff) return std::min(height, SEA_LEVEL - CLIFF_DEPTH);
            }
        }
    }
    return height;
}

int TerrainGenerator::TerrainHeightAt(int wx, int wz) {
    switch (TileKindAt(ColumnToTile(wx), ColumnToTile(wz))) {
    case TileKind::Land:
        return SEA_LEVEL + ISLAND_HEIGHT;
    case TileKind::Beach: {
        // Slopes down with the distance to the nearest land tile (square, so corners stay square)
        float along = (float)(LandDistance(wx, wz, 1) - 1) / (float)(TILE_SIZE - 1);
        return (int)std::lround(glm::mix((float)BEACH_TOP, (float)BEACH_BOTTOM, std::min(along, 1.0f)));
    }
    case TileKind::Cliff:
    case TileKind::Sea:
    default:
        return SeaFloorHeight(wx, wz);
    }
}

// Everything that is not land is sand: beaches and the sea floor
bool TerrainGenerator::IsSandy(int wx, int wz) {
    return !IsLandTile(ColumnToTile(wx), ColumnToTile(wz));
}

bool TerrainGenerator::FacesCliff(int wx, int wz) {
    int tx = ColumnToTile(wx), tz = ColumnToTile(wz);
    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            int ntx = ColumnToTile(wx + dx), ntz = ColumnToTile(wz + dz);
            if ((ntx != tx || ntz != tz) && TileKindAt(ntx, ntz) == TileKind::Cliff) return true;
        }
    }
    return false;
}

void TerrainGenerator::GenerateChunk(int cx, int cy, int cz, std::vector<uint8_t>& data) {
    data.assign(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, 0);
    int startX = cx * CHUNK_SIZE;
    int startY = cy * CHUNK_SIZE;
    int startZ = cz * CHUNK_SIZE;

    // Chunks wholly above the ground and the sea skip the columns: only tree tops reach there
    for (int z = 0; z < CHUNK_SIZE && startY < GROUND_TOP; z++) {
        int wz = startZ + z;
        for (int x = 0; x < CHUNK_SIZE; x++) {
            int wx = startX + x;

            int biome = BiomeFromNoise(BiomeNoise(wx, wz));
            int terrainHeight = TerrainHeightAt(wx, wz);
            bool sandy = IsSandy(wx, wz);
            // The cliff face: rock under the grass along the edge of a land tile toward a cliff
            int localX = wx - ColumnToTile(wx) * TILE_SIZE, localZ = wz - ColumnToTile(wz) * TILE_SIZE;
            bool onEdge = localX == 0 || localZ == 0 || localX == TILE_SIZE - 1 || localZ == TILE_SIZE - 1;
            bool cliffFace = !sandy && onEdge && FacesCliff(wx, wz);

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
                    blockID = cliffFace ? Block::STONE : Block::DIRT;
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
    if (!m_Foliage.IsEmpty() && startY < GROUND_TOP + m_Foliage.Height()) StampTrees(startX, startY, startZ, data);

    // Grass tufts in their rest pose, after trees so they only fill air
    if (GRASS_TUFTS_ENABLED) StampGrass(startX, startY, startZ, data);
}

namespace {

uint32_t CellHash(int cx, int cz, uint32_t salt) {
    uint32_t h = (uint32_t)cx * 0x8DA6B343u ^ (uint32_t)cz * 0xD8163841u ^ salt;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    return h ^ (h >> 15);
}

// The cell's spot, a column at least one in from the cell's edges
glm::ivec2 CellSpot(int cx, int cz, int cell, uint32_t hash) {
    return glm::ivec2(cx * cell + 1 + (int)(hash % (uint32_t)(cell - 2)), cz * cell + 1 + (int)((hash >> 8) % (uint32_t)(cell - 2)));
}

float Chance(uint32_t hash) { return (float)(hash >> 20) / 4096.0f; }

constexpr uint32_t TREE_SALT = 0x7EE5u, DECOR_SALT = 0xDEC0u;
constexpr int FOLIAGE_LAND_MARGIN = 3; // Land this many columns round a root, so nothing hangs over the beach

} // namespace

float TerrainGenerator::ForestNoise(int wx, int wz) {
    float noise = m_ForestNoise.GetNoise((float)wx, (float)wz);
    return std::clamp(noise / 0.25f + 0.5f, 0.0f, 1.0f); // Forest above 0, with a short edge
}

int TerrainGenerator::PickKind(bool trees, glm::ivec2 root, float forest, uint32_t hash) {
    if (m_Foliage.IsEmpty()) return -1;
    for (glm::ivec2 d : { glm::ivec2(0), glm::ivec2(FOLIAGE_LAND_MARGIN, 0), glm::ivec2(-FOLIAGE_LAND_MARGIN, 0), glm::ivec2(0, FOLIAGE_LAND_MARGIN),
                          glm::ivec2(0, -FOLIAGE_LAND_MARGIN) }) {
        if (IsSandy(root.x + d.x, root.y + d.y)) return -1;
    }
    int session = SessionAtColumn(root.x, root.y);
    Climate climate = session >= 0 && SESSIONS[session].tropical ? Climate::Tropical : Climate::Temperate;
    bool inForest = forest > 0.5f;
    auto weight = [&](const FoliageKind& kind) {
        if ((kind.kind == FoliageClass::Tree) != trees || (kind.climate != climate && kind.climate != Climate::Both)) return 0;
        return (int)(inForest ? kind.forestWeight : kind.meadowWeight);
    };
    int total = 0;
    for (const FoliageKind& kind : FOLIAGE_KINDS) total += weight(kind);
    if (total == 0) return -1;
    int pick = (int)((hash >> 4) % (uint32_t)total);
    for (int i = 0; i < (int)FOLIAGE_KINDS.size(); i++) {
        pick -= weight(FOLIAGE_KINDS[i]);
        if (pick < 0) return FoliageModelIndex(i, (int)(hash >> 30));
    }
    return -1;
}

int TerrainGenerator::TreeInCell(int cx, int cz, glm::ivec2& root) {
    uint32_t hash = CellHash(cx, cz, TREE_SALT);
    root = CellSpot(cx, cz, TREE_CELL, hash);
    float forest = ForestNoise(root.x, root.y);
    int session = SessionAtColumn(root.x, root.y);
    float thin = session >= 0 && SESSIONS[session].tropical ? 0.55f : 1.0f; // Palm groves stand apart, so their crowns show
    if (Chance(hash) >= (0.04f + 0.86f * forest) * thin) return -1; // Nearly always in a forest, seldom in a meadow
    return PickKind(true, root, forest, CellHash(cx, cz, TREE_SALT + 1));
}

int TerrainGenerator::DecorInCell(int cx, int cz, glm::ivec2& root) {
    uint32_t hash = CellHash(cx, cz, DECOR_SALT);
    root = CellSpot(cx, cz, DECOR_CELL, hash);
    if (Chance(hash) >= 0.4f) return -1;
    // Not at a tree's foot: the tree cell's spot, whether a tree grows there or not
    int tcx = FloorDiv(root.x, TREE_CELL), tcz = FloorDiv(root.y, TREE_CELL);
    glm::ivec2 tree = CellSpot(tcx, tcz, TREE_CELL, CellHash(tcx, tcz, TREE_SALT));
    if (std::abs(tree.x - root.x) <= 2 && std::abs(tree.y - root.y) <= 2) return -1;
    return PickKind(false, root, ForestNoise(root.x, root.y), CellHash(cx, cz, DECOR_SALT + 1));
}

int TerrainGenerator::FoliageAt(int wx, int wz) {
    glm::ivec2 root;
    int model = TreeInCell(FloorDiv(wx, TREE_CELL), FloorDiv(wz, TREE_CELL), root);
    if (model >= 0 && root == glm::ivec2(wx, wz)) return model;
    model = DecorInCell(FloorDiv(wx, DECOR_CELL), FloorDiv(wz, DECOR_CELL), root);
    return model >= 0 && root == glm::ivec2(wx, wz) ? model : -1;
}

const VoxModel& TerrainGenerator::FoliageModel(int model) const {
    return m_Foliage.Model(model);
}

int TerrainGenerator::FoliageReach() const {
    return m_Foliage.Reach();
}

// Roots are scanned in a margin around the chunk so foliage rooted in a neighbour stamps its
// overhanging part here too, keeping it whole across chunk borders
void TerrainGenerator::StampTrees(int startX, int startY, int startZ, std::vector<uint8_t>& data) {
    int reach = m_Foliage.Reach();
    ForEachFoliageRoot(glm::ivec2(startX, startZ) - reach, glm::ivec2(startX, startZ) + CHUNK_SIZE - 1 + reach, [&](glm::ivec2 root, int index) {
        if (m_Felled && m_Felled->Contains(root)) return; // Cut down by a lumberjack, or cleared by a building
        const VoxModel& model = m_Foliage.Model(index);
        int rx = root.x - startX, rz = root.y - startZ;
        if (rx + model.max.x < 0 || rx + model.min.x >= CHUNK_SIZE || rz + model.max.z < 0 || rz + model.min.z >= CHUNK_SIZE) return;
        int localY = TreeRootY(root.x, root.y) - startY;
        if (localY + model.max.y < 0 || localY + model.min.y >= CHUNK_SIZE) return;
        for (const VoxelOffset& offset : model.voxels) {
            int tx = rx + offset.x;
            int ty = localY + offset.y;
            int tz = rz + offset.z;
            if (tx >= 0 && tx < CHUNK_SIZE && ty >= 0 && ty < CHUNK_SIZE && tz >= 0 && tz < CHUNK_SIZE) {
                data[LocalIndex(tx, ty, tz)] = (uint8_t)offset.blockType;
            }
        }
    });
}

// Decided only by noise and a hash of the cell, so every chunk (and the main thread) agrees on
// where tufts are
bool TerrainGenerator::GrassTuftInCell(int gx, int gz, GrassTuft& tuft) {
    uint32_t h = GrassCellHash(gx, gz);
    if ((h & 0xFF) < 40) return false; // ~15% of cells stay bare, for natural gaps

    int wx = gx * GRASS_CELL + (int)((h >> 8) & (GRASS_CELL - 1));
    int wz = gz * GRASS_CELL + (int)((h >> 10) & (GRASS_CELL - 1));
    if (BiomeNoise(wx, wz) < -0.2f) return false; // No tufts in the Crystalline Peaks biome

    if (IsSandy(wx, wz)) return false; // Beaches and the sea floor are sand, not grass
    int terrainHeight = TerrainHeightAt(wx, wz);

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
    const int STEP = 48, PROBE = 72, MAX_RINGS = 400;
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
