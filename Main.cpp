#define _CRT_SECURE_NO_WARNINGS
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// --- Helper Math Includes (GLM) ---
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "FastNoiseLite.h"
#include "rayCast.h"
#include "chunk.h"
#include "animation.hpp"
#include "modelLoader.hpp"
#include "Ocean.h"
#include "Shore.h"

// --- Visual Studio Linker Config ---
#ifdef _MSC_VER
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "opengl32.lib")
#endif


// --- Configuration ---
const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;
const float REACH_DISTANCE = 10.0f;
const float PLAYER_RADIUS = 0.45f / VOXELS_PER_UNIT; // Drone boundary radius (slightly smaller than 0.5 voxels for fitting in 1-voxel gaps)
const glm::ivec2 SPAWN_SEARCH_START(640, 640);      // Spawn is the nearest decent island to here
glm::ivec3 spawnVoxel(640, 0, 640);                  // Chosen at startup (beacon in default.comp sits here)

// Ocean surface. 42 keeps the wave layer (y = 43) in the same 8^3 brick and 32^3 chunk as the
// water below it, so the ray marcher never skips past a raised wave (see seaWave in default.comp).
const int SEA_LEVEL = 42;
const int ISLAND_HEIGHT = 4; // Islands are flat, this many voxels above the sea

// Gameplay tuning was authored per frame at 165 FPS; these convert it to per second
const float REFERENCE_FPS = 165.0f;

// --- Chunk Streaming Radii (in chunks, Chebyshev distance on XZ) ---
// Far chunks live only on the GPU. The CPU keeps voxel data just around the player (collision,
// raycasts, editing, minimap, scanner) plus every chunk the player has edited, so RAM use does
// not grow with the render distance.
const int MIN_RENDER_DISTANCE = 4;
const int MAX_RENDER_DISTANCE = 128;
const int GPU_EVICT_MARGIN = 2;      // GPU chunks are evicted this far past the render distance
const int R_CPU = 16;                // CPU voxel data is loaded within this radius...
const int R_CPU_KEEP = R_CPU + 2;    // ...and freed (unless edited) beyond this one
int R_ACTIVE = 18;                   // Render distance: GPU upload radius
std::atomic<int> workerCancelRadius(std::max(R_ACTIVE + GPU_EVICT_MARGIN, R_CPU_KEEP)); // Requests beyond this are skipped

// --- Camera State (Globals for Callback Access) ---
glm::vec3 cameraPos = glm::vec3(0.5f, 0.09f, 0.5f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
int selectedBlockID = 1; // 1 = Grass, 2 = Dirt, 3 = Stone, 4 = Sand, 5 = Plant

// --- GPU Chunk System Globals ---
// Toroidal page table: PAGE_TABLE_WRAP x 4 x PAGE_TABLE_WRAP entries of [slotIndex+1, cx, cy, cz].
// Must be wider than the GPU window (2 * (MAX_RENDER_DISTANCE + GPU_EVICT_MARGIN) + 1 = 261).
const int PAGE_TABLE_WRAP = 512;
GLuint pageTableTex = 0;
std::vector<glm::ivec4> pageTableData(PAGE_TABLE_WRAP * CHUNK_LAYERS * PAGE_TABLE_WRAP, glm::ivec4(0));

// Voxel data lives in up to MAX_POOLS "pool" 3D textures, each a 64 x 64 x (32 * layers) grid
// of 32^3 chunk slots (one layer = 4096 slots = 128 MB). A new pool is allocated only when the
// previous ones are full, so VRAM follows the render distance, and existing slots never move
// (no copying and no 2x memory spike while growing). Each pool has a brick mask beside it:
// one texel per 8^3 brick, non-zero if the brick has any solid voxel.
const int BRICK_SIZE = 8;
const int BRICKS_PER_AXIS = CHUNK_SIZE / BRICK_SIZE; // 4
const int POOL_SLOTS_XY = 64;
const int SLOTS_PER_LAYER = POOL_SLOTS_XY * POOL_SLOTS_XY;
const int MAX_POOLS = 4;
const int POOL_PLANNED_LAYERS[MAX_POOLS] = { 2, 6, 16, 32 }; // 256 MB, 768 MB, 2 GB, 4 GB
const GLenum POOL_TEXTURE_UNITS[MAX_POOLS] = { GL_TEXTURE2, GL_TEXTURE4, GL_TEXTURE6, GL_TEXTURE8 };
const GLenum BRICK_TEXTURE_UNITS[MAX_POOLS] = { GL_TEXTURE3, GL_TEXTURE5, GL_TEXTURE7, GL_TEXTURE9 };

struct ChunkPool {
    GLuint voxels = 0;
    GLuint bricks = 0;
    int baseSlot = 0; // First global slot index stored in this pool
    int layers = 0;
};
ChunkPool pools[MAX_POOLS];
int poolCount = 0;
bool poolsExhausted = false; // Out of pools or VRAM; further chunks are not uploaded

std::unordered_map<uint64_t, int> chunkSlotMap;      // Maps chunk key to slot index in physical texture pool
std::unordered_map<uint64_t, Chunk*> worldMap;       // CPU voxel data (near the player, or edited)
std::vector<int> freeSlots;
std::unordered_set<uint64_t> inFlightChunks;         // Requested from the workers, result not processed yet
std::unordered_set<uint64_t> knownEmptyChunks;       // All-air chunks in the GPU window (need no slot)
glm::ivec3 playerChunk(0);

// --- Project Strata Globals ---
struct FlareProjectile {
    glm::vec3 pos;
    glm::vec3 vel; // world units / second
    glm::vec3 color;
    float intensity;
    bool isStuck;
    float life;
};
std::vector<FlareProjectile> activeFlares;

struct ClumpProjectile {
    glm::vec3 pos;
    glm::vec3 vel; // world units / second
    float radius;
    bool active;
};
std::vector<ClumpProjectile> activeClumpProjectiles;

bool heldClumpActive = false;
bool heldClumpIsArtifact = false;
glm::vec3 heldClumpPos = glm::vec3(0.0f);
float heldClumpRadius = 1.5f;

bool laserBeamActive = false;
glm::vec3 laserBeamStart = glm::vec3(0.0f);
glm::vec3 laserBeamEnd = glm::vec3(0.0f);
glm::vec3 laserBeamColor = glm::vec3(1.0f, 0.0f, 0.0f);

int playerArtifactsRetrieved = 0;
float closestArtifactDistance = -1.0f;

GLuint minimapTex = 0;
std::vector<GLubyte> minimapData(64 * 64 * 4, 0);

glm::vec3 playerVelocity = glm::vec3(0.0f);

bool debugOverlayEnabled = true;
bool paintModeEnabled = false;
bool chunkViewerEnabled = false;
bool lightVisualizerEnabled = false;
bool grassListDirty = true; // Animated grass list needs rebuilding (player moved or terrain edited)

// --- Render Quality ---
const float MIN_RENDER_SCALE = 0.5f;
float renderScale = 1.0f;          // Fraction of the window resolution the rays are traced at
bool halfResShadowsEnabled = true; // Soft shadows once per 2x2 pixel quad instead of per pixel

bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = WINDOW_WIDTH / 2.0f;
float lastY = WINDOW_HEIGHT / 2.0f;

const std::string treeModelPath = "assets/tree.vox";
std::vector<Offset> treeOffsets;
glm::ivec3 treeMin(0), treeMax(0); // Bounds of treeOffsets

inline int chunkDistXZ(int cx, int cz) {
    return std::max(std::abs(cx - playerChunk.x), std::abs(cz - playerChunk.z));
}

// --- Background Generation ---

template <typename T>
class SafeQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cv;
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(value));
        cv.notify_one();
    }
    bool pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) return false;
        value = std::move(queue.front());
        queue.pop();
        return true;
    }
    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this]() { return !queue.empty(); });
        value = std::move(queue.front());
        queue.pop();
    }
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
};

struct ChunkRequest {
    int cx, cy, cz;
};

struct ChunkResult {
    int cx, cy, cz;
    std::vector<GLubyte> data;         // Empty when the chunk is all air
    std::vector<uint16_t> artifactIdx;
    GLubyte brickMask[BRICKS_PER_AXIS * BRICKS_PER_AXIS * BRICKS_PER_AXIS] = {};
    bool cancelled = false; // Player moved away before the worker got to it
};

// Fills the chunk's 4x4x4 brick occupancy (255 = has a solid voxel) and returns whether any
// voxel is solid. Layout: x fastest, then y, then z (matches glTexSubImage3D).
bool computeBrickMask(const std::vector<GLubyte>& data, GLubyte* brickMask) {
    std::fill(brickMask, brickMask + BRICKS_PER_AXIS * BRICKS_PER_AXIS * BRICKS_PER_AXIS, 0);
    if (data.empty()) return false;
    bool hasBlocks = false;
    for (int z = 0; z < CHUNK_SIZE; z++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                if (data[localIndex(x, y, z)] > 0) {
                    brickMask[((z / BRICK_SIZE) * BRICKS_PER_AXIS + (y / BRICK_SIZE)) * BRICKS_PER_AXIS + (x / BRICK_SIZE)] = 255;
                    hasBlocks = true;
                }
            }
        }
    }
    return hasBlocks;
}

SafeQueue<ChunkRequest> requestQueue;
SafeQueue<ChunkResult> resultQueue;
std::atomic<bool> workerRunning(true);
std::atomic<int> workerPlayerCx(0), workerPlayerCz(0);
std::vector<std::thread> workerThreads;

struct TerrainGen {
    FastNoiseLite noise;
    FastNoiseLite biomeNoise;
    FastNoiseLite caveNoise;
    FastNoiseLite islandNoise;
    float noiseScale = 0.5f;

    TerrainGen() {
        noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

        // Low-frequency mask deciding land vs ocean: islands a few hundred voxels across
        islandNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        islandNoise.SetFrequency(0.0025f);
        islandNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        islandNoise.SetFractalOctaves(3);
        islandNoise.SetSeed(7);

        biomeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        biomeNoise.SetFrequency(0.005f); // Low frequency for large scale biomes

        caveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        caveNoise.SetFrequency(0.035f); // Tunnel-scale cave frequency
    }

    int biomeAt(float bn) const {
        if (bn < -0.2f) return 1; // Crystalline Peaks
        if (bn > 0.2f) return 2;  // Verdant Caverns
        return 3;                 // Default: Crust
    }

    // 0 = open ocean, 1 = island interior, smooth in between (the coast)
    float islandMask(int wx, int wz) {
        float n = islandNoise.GetNoise((float)wx, (float)wz);
        float t = glm::clamp(n / 0.3f, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Blends the sea floor (~16 below sea level) into flat island ground ISLAND_HEIGHT above
    // sea level; the blend band is the coast, which slopes down through the beach.
    // (biome is unused here now that islands are flat; it still picks caves and artifacts)
    int terrainHeightAt(int wx, int wz, int /*biome*/) {
        float heightSample = noise.GetNoise((float)wx * noiseScale, (float)wz * noiseScale);
        float seaFloor = SEA_LEVEL - 16.0f + heightSample * 4.0f;
        float landTop = (float)(SEA_LEVEL + ISLAND_HEIGHT);
        return static_cast<int>(glm::mix(seaFloor, landTop, islandMask(wx, wz)));
    }

    // The sea floor, and beaches along the coast (outer part of the island mask, up to a few
    // voxels above the sea), are sand
    bool isSandy(int wx, int wz, int terrainHeight) {
        if (terrainHeight <= SEA_LEVEL + 2) return true;
        return terrainHeight <= SEA_LEVEL + 6 && islandMask(wx, wz) < 0.3f;
    }

    void generateChunk(int cx, int cy, int cz, std::vector<GLubyte>& data, std::vector<uint16_t>& artifactIdx) {
        data.assign(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, 0);
        artifactIdx.clear();
        int startX = cx * CHUNK_SIZE;
        int startY = cy * CHUNK_SIZE;
        int startZ = cz * CHUNK_SIZE;

        for (int z = 0; z < CHUNK_SIZE; z++) {
            int wz = startZ + z;
            for (int x = 0; x < CHUNK_SIZE; x++) {
                int wx = startX + x;

                int biome = biomeAt(biomeNoise.GetNoise((float)wx, (float)wz));
                int terrainHeight = terrainHeightAt(wx, wz, biome);

                bool sandy = isSandy(wx, wz, terrainHeight);
                for (int y = 0; y < CHUNK_SIZE; y++) {
                    int wy = startY + y;
                    if (wy >= terrainHeight) {
                        // Ocean: everything above the ground up to sea level is water
                        if (wy <= SEA_LEVEL) data[localIndex(x, y, z)] = WATER_ID;
                        continue;
                    }

                    GLubyte blockID = 3; // Stone
                    if (sandy && wy > terrainHeight - 4) {
                        blockID = 4; // Sand: beaches and the sea floor
                    }
                    else if (wy == terrainHeight - 1) {
                        blockID = 1; // Grass
                    }
                    else if (wy > terrainHeight - 4) {
                        blockID = 2; // Dirt
                    }

                    // Cave carving using abs(noise) for worm/tunnel shapes
                    if (wy > 12 && wy < terrainHeight - 12) {
                        float n3d = caveNoise.GetNoise((float)wx, (float)wy, (float)wz);
                        float caveVal = fabs(n3d); // abs() creates tunnel shapes near zero-crossings
                        // Verdant Caverns: wider tunnels with bioluminescent walls
                        if (biome == 2) {
                            if (caveVal < 0.06f) {
                                blockID = 0; // Empty cavern tunnel
                            } else if (caveVal < 0.09f) {
                                blockID = 30; // Bioluminescent cavern walls
                            }
                        }
                        // Crust biome: narrow tunnel caves
                        else if (biome == 3) {
                            if (caveVal < 0.04f) {
                                blockID = 0; // Narrow cave tunnel
                            }
                        }
                    }

                    // Crust Biome & Artifacts
                    if (biome == 3 && blockID == 3 && wy > 15 && wy < 45) {
                        float artSample = noise.GetNoise((float)wx * 5.0f, (float)wy * 5.0f, (float)wz * 5.0f);
                        if (artSample > 0.96f) {
                            blockID = 32; // Artifact block
                        }
                    }

                    size_t idx = localIndex(x, y, z);
                    data[idx] = blockID;
                    if (blockID == 32) artifactIdx.push_back((uint16_t)idx);
                }
            }
        }

        // Spawn trees deterministically on Grass top-blocks (only in Caverns or Crust biomes).
        // Roots are scanned in a margin around the chunk so trees rooted in a neighbour
        // stamp their overhanging part here too, keeping trees whole across chunk borders.
        if (!treeOffsets.empty()) stampTrees(startX, startY, startZ, data);

        // Grass tufts in their rest pose, after trees so they only fill air
        if (GRASS_TUFTS_ENABLED) stampGrass(startX, startY, startZ, data);
    }

    void stampTrees(int startX, int startY, int startZ, std::vector<GLubyte>& data) {
        for (int rz = -treeMax.z; rz < CHUNK_SIZE - treeMin.z; rz++) {
            int wz = startZ + rz;
            for (int rx = -treeMax.x; rx < CHUNK_SIZE - treeMin.x; rx++) {
                int wx = startX + rx;

                float bn = biomeNoise.GetNoise((float)wx, (float)wz);
                if (bn < -0.2f) continue; // No trees in the Crystalline Peaks biome (open grassland)

                float treeNoise = noise.GetNoise((float)wx * 15.0f, (float)wz * 15.0f);
                if (treeNoise <= 0.85f) continue;

                int rootY = terrainHeightAt(wx, wz, biomeAt(bn)) - 1;
                if (isSandy(wx, wz, rootY + 1)) continue; // No trees on beaches or underwater
                int localY = rootY - startY;
                if (localY + treeMax.y < 0 || localY + treeMin.y >= CHUNK_SIZE) continue;

                for (const auto& off : treeOffsets) {
                    int tx = rx + off.x;
                    int ty = localY + off.y;
                    int tz = rz + off.z;
                    if (tx >= 0 && tx < CHUNK_SIZE && ty >= 0 && ty < CHUNK_SIZE && tz >= 0 && tz < CHUNK_SIZE) {
                        data[localIndex(tx, ty, tz)] = (GLubyte)off.blockType;
                    }
                }
            }
        }
    }

    // One candidate tuft per GRASS_CELL x GRASS_CELL cell of columns, decided only by noise and a
    // hash of the cell, so every chunk (and the main thread) agrees on where tufts are.
    bool grassTuftInCell(int gx, int gz, GrassTuft& tuft) {
        uint32_t h = grassCellHash(gx, gz);
        if ((h & 0xFF) < 40) return false; // ~15% of cells stay bare, for natural gaps

        int wx = gx * GRASS_CELL + (int)((h >> 8) & (GRASS_CELL - 1));
        int wz = gz * GRASS_CELL + (int)((h >> 10) & (GRASS_CELL - 1));
        float bn = biomeNoise.GetNoise((float)wx, (float)wz);
        if (bn < -0.2f) return false; // No tufts in the Crystalline Peaks biome

        int terrainHeight = terrainHeightAt(wx, wz, biomeAt(bn));
        if (isSandy(wx, wz, terrainHeight)) return false; // Beaches and the sea floor are sand, not grass

        tuft.root = glm::ivec3(wx, terrainHeight, wz);
        tuft.variant = (int)((h >> 12) & 3);
        tuft.phase = (float)((h >> 14) & 255) / 255.0f * 0.7f;
        return true;
    }

    void stampGrass(int startX, int startY, int startZ, std::vector<GLubyte>& data) {
        // Blades sit up to 1 voxel off their root, so scan cells 1 voxel past the chunk border
        for (int gz = (startZ - 1) >> 2; gz <= (startZ + CHUNK_SIZE) >> 2; gz++) {
            for (int gx = (startX - 1) >> 2; gx <= (startX + CHUNK_SIZE) >> 2; gx++) {
                GrassTuft tuft;
                if (!grassTuftInCell(gx, gz, tuft)) continue;
                if (tuft.root.y + GRASS_MAX_HEIGHT <= startY || tuft.root.y >= startY + CHUNK_SIZE) continue;

                for (int b = 0; b < GRASS_BLADES; b++) {
                    const int* blade = GRASS_BLADE_SHAPES[tuft.variant][b];
                    for (int lv = 0; lv < blade[2]; lv++) {
                        int lx = tuft.root.x + blade[0] - startX;
                        int ly = tuft.root.y + lv - startY;
                        int lz = tuft.root.z + blade[1] - startZ;
                        if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_SIZE) continue;
                        GLubyte& voxel = data[localIndex(lx, ly, lz)];
                        if (voxel == 0) voxel = GRASS_TUFT_ID;
                    }
                }
            }
        }
    }
};

void chunkGeneratorWorker() {
    TerrainGen gen;

    while (workerRunning) {
        ChunkRequest req;
        requestQueue.wait_and_pop(req);
        if (!workerRunning) break;

        ChunkResult res;
        res.cx = req.cx; res.cy = req.cy; res.cz = req.cz;

        // Skip requests the player has already flown away from
        int dist = std::max(std::abs(req.cx - workerPlayerCx.load()), std::abs(req.cz - workerPlayerCz.load()));
        if (dist > workerCancelRadius.load()) {
            res.cancelled = true;
        } else {
            gen.generateChunk(req.cx, req.cy, req.cz, res.data, res.artifactIdx);
            // Done here so the main thread only has to upload
            if (!computeBrickMask(res.data, res.brickMask)) std::vector<GLubyte>().swap(res.data);
        }
        resultQueue.push(std::move(res));
    }
}

// --- GPU Residency ---

void writePageTableTexel(int cx, int cy, int cz, glm::ivec4 value) {
    int ptX = cx % PAGE_TABLE_WRAP; if (ptX < 0) ptX += PAGE_TABLE_WRAP;
    int ptZ = cz % PAGE_TABLE_WRAP; if (ptZ < 0) ptZ += PAGE_TABLE_WRAP;
    pageTableData[(ptZ * CHUNK_LAYERS + cy) * PAGE_TABLE_WRAP + ptX] = value;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, pageTableTex);
    glTexSubImage3D(GL_TEXTURE_3D, 0, ptX, cy, ptZ, 1, 1, 1, GL_RGBA_INTEGER, GL_INT, &value);
}

const glm::ivec4& readPageTable(int cx, int cy, int cz) {
    int ptX = cx % PAGE_TABLE_WRAP; if (ptX < 0) ptX += PAGE_TABLE_WRAP;
    int ptZ = cz % PAGE_TABLE_WRAP; if (ptZ < 0) ptZ += PAGE_TABLE_WRAP;
    return pageTableData[(ptZ * CHUNK_LAYERS + cy) * PAGE_TABLE_WRAP + ptX];
}

// Allocates an empty R8 3D texture with nearest filtering
void allocate3DTexture(GLuint tex, int width, int height, int depth) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_R8, width, height, depth);
}

// Adds the next pool texture (and its brick mask) and puts its slots on the free list.
// Halves the planned size if the driver reports out of memory.
bool addChunkPool() {
    if (poolCount == MAX_POOLS) {
        poolsExhausted = true;
        return false;
    }
    int baseSlot = poolCount == 0 ? 0 : pools[poolCount - 1].baseSlot + pools[poolCount - 1].layers * SLOTS_PER_LAYER;

    for (int layers = POOL_PLANNED_LAYERS[poolCount]; layers >= 1; layers /= 2) {
        GLuint tex[2];
        glGenTextures(2, tex);
        while (glGetError() != GL_NO_ERROR) {} // Clear stale errors so we only see ours
        allocate3DTexture(tex[0], POOL_SLOTS_XY * CHUNK_SIZE, POOL_SLOTS_XY * CHUNK_SIZE, layers * CHUNK_SIZE);
        allocate3DTexture(tex[1], POOL_SLOTS_XY * BRICKS_PER_AXIS, POOL_SLOTS_XY * BRICKS_PER_AXIS, layers * BRICKS_PER_AXIS);
        if (glGetError() == GL_OUT_OF_MEMORY) {
            glDeleteTextures(2, tex);
            continue;
        }

        pools[poolCount] = { tex[0], tex[1], baseSlot, layers };
        poolCount++;
        // Push in reverse so the lowest new slot is handed out first
        for (int s = baseSlot + layers * SLOTS_PER_LAYER - 1; s >= baseSlot; s--) {
            freeSlots.push_back(s);
        }
        std::cout << "Allocated chunk pool " << poolCount << ": " << layers * SLOTS_PER_LAYER
                  << " slots (" << layers * 128 << " MB)" << std::endl;
        return true;
    }

    std::cerr << "Out of video memory for chunk pools; distant chunks will not be shown." << std::endl;
    poolsExhausted = true;
    return false;
}

// First global slot of pools 1..3 for the shaders (INT_MAX = pool not allocated)
glm::ivec3 poolBaseSlots() {
    glm::ivec3 base(INT_MAX);
    for (int p = 1; p < poolCount; p++) base[p - 1] = pools[p].baseSlot;
    return base;
}

// Finds the pool holding a slot and the slot's chunk origin (in voxels) inside it
int locateSlot(int slotIndex, glm::ivec3& originVoxels) {
    int p = poolCount - 1;
    while (p > 0 && slotIndex < pools[p].baseSlot) p--;
    int local = slotIndex - pools[p].baseSlot;
    originVoxels = glm::ivec3(local % POOL_SLOTS_XY, (local / POOL_SLOTS_XY) % POOL_SLOTS_XY, local / SLOTS_PER_LAYER) * CHUNK_SIZE;
    return p;
}

void uploadChunkToSlot(int slotIndex, const std::vector<GLubyte>& data, const GLubyte* brickMask) {
    glm::ivec3 origin;
    int p = locateSlot(slotIndex, origin);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, pools[p].voxels);
    glTexSubImage3D(GL_TEXTURE_3D, 0, origin.x, origin.y, origin.z, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, GL_RED, GL_UNSIGNED_BYTE, data.data());

    glm::ivec3 brickOrigin = origin / BRICK_SIZE;
    glBindTexture(GL_TEXTURE_3D, pools[p].bricks);
    glTexSubImage3D(GL_TEXTURE_3D, 0, brickOrigin.x, brickOrigin.y, brickOrigin.z,
        BRICKS_PER_AXIS, BRICKS_PER_AXIS, BRICKS_PER_AXIS, GL_RED, GL_UNSIGNED_BYTE, brickMask);
}

void evictResident(uint64_t key) {
    auto it = chunkSlotMap.find(key);
    if (it == chunkSlotMap.end()) return;
    freeSlots.push_back(it->second);
    chunkSlotMap.erase(it);

    int cx, cy, cz;
    unpackChunkKey(key, cx, cy, cz);

    // Only clear the page table if it still points at this chunk (not a wrapped neighbour)
    const glm::ivec4& entry = readPageTable(cx, cy, cz);
    if (entry.y == cx && entry.z == cy && entry.w == cz) {
        writePageTableTexel(cx, cy, cz, glm::ivec4(0));
    }
}

// Uploads chunk data into the GPU pool, allocating a slot if it has none yet.
// brickMask may be null, in which case it is computed here (used after edits).
// All-air chunks get no slot and are remembered in knownEmptyChunks instead.
void makeResident(uint64_t key, glm::ivec3 chunkCoord, const std::vector<GLubyte>& data, const GLubyte* brickMask) {
    GLubyte computedMask[BRICKS_PER_AXIS * BRICKS_PER_AXIS * BRICKS_PER_AXIS];
    bool hasBlocks;
    if (brickMask) {
        hasBlocks = !data.empty();
    } else {
        hasBlocks = computeBrickMask(data, computedMask);
        brickMask = computedMask;
    }

    auto it = chunkSlotMap.find(key);
    if (!hasBlocks) {
        if (it != chunkSlotMap.end()) evictResident(key); // Dug out completely
        knownEmptyChunks.insert(key);
        return;
    }
    knownEmptyChunks.erase(key);

    if (it != chunkSlotMap.end()) {
        uploadChunkToSlot(it->second, data, brickMask);
        return;
    }

    if (freeSlots.empty() && (poolsExhausted || !addChunkPool())) return;

    int slotIndex = freeSlots.back();
    freeSlots.pop_back();
    chunkSlotMap[key] = slotIndex;

    uploadChunkToSlot(slotIndex, data, brickMask);
    writePageTableTexel(chunkCoord.x, chunkCoord.y, chunkCoord.z,
        glm::ivec4(slotIndex + 1, chunkCoord.x, chunkCoord.y, chunkCoord.z));
}

// Re-uploads an edited chunk, or makes it resident if it is close enough
void refreshChunk(uint64_t key) {
    auto it = worldMap.find(key);
    if (it == worldMap.end()) return;
    Chunk* chunk = it->second;
    if (chunkSlotMap.count(key) || chunkDistXZ(chunk->position.x, chunk->position.z) <= R_ACTIVE) {
        makeResident(key, chunk->position, chunk->data, nullptr);
    }
}

void requestChunk(int cx, int cy, int cz) {
    if (inFlightChunks.insert(getChunkKey(cx, cy, cz)).second) {
        requestQueue.push({ cx, cy, cz });
    }
}

// Whether the current windows want this chunk at all (CPU copy or GPU upload)
bool chunkIsWanted(int cx, int cz) {
    int dist = chunkDistXZ(cx, cz);
    return dist <= R_CPU || dist <= R_ACTIVE;
}

void integrateResult(ChunkResult& res) {
    uint64_t key = getChunkKey(res.cx, res.cy, res.cz);
    inFlightChunks.erase(key);

    if (res.cancelled) {
        // The player may have come back since the worker skipped it
        if (chunkIsWanted(res.cx, res.cz)) requestChunk(res.cx, res.cy, res.cz);
        return;
    }

    int dist = chunkDistXZ(res.cx, res.cz);
    Chunk* cpuChunk = findChunk(res.cx, res.cy, res.cz);

    // GPU: upload if in the render distance; an edited CPU copy always wins over generated data
    bool wantGpu = dist <= R_ACTIVE && !chunkSlotMap.count(key) && !knownEmptyChunks.count(key);
    if (wantGpu) {
        glm::ivec3 coord(res.cx, res.cy, res.cz);
        if (cpuChunk && cpuChunk->isModified) {
            makeResident(key, coord, cpuChunk->data, nullptr);
        } else {
            makeResident(key, coord, res.data, res.brickMask);
        }
    }

    // CPU: keep voxel data only close to the player
    if (!cpuChunk && dist <= R_CPU) {
        Chunk* chunk = getOrCreateChunk(res.cx, res.cy, res.cz);
        chunk->data = std::move(res.data);
        chunk->artifactIdx = std::move(res.artifactIdx);
    }
}

// --- Streaming Windows ---
// Each window is a square of chunk columns around the player. When the player moves or the
// render distance changes, only the columns entering or leaving a window are processed,
// so the cost does not scale with the window's area.

struct StreamWindow {
    glm::ivec2 center = glm::ivec2(0);
    int radius = -1; // -1 = empty
    bool contains(int cx, int cz) const {
        return radius >= 0 && std::abs(cx - center.x) <= radius && std::abs(cz - center.y) <= radius;
    }
};

StreamWindow gpuLoadWindow, gpuKeepWindow, cpuLoadWindow, cpuKeepWindow;

// Columns inside `to` but not inside `from`, nearest to `to`'s center first
std::vector<glm::ivec2> columnsEntering(const StreamWindow& from, const StreamWindow& to) {
    std::vector<glm::ivec2> cols;
    if (to.radius < 0) return cols;
    for (int dz = -to.radius; dz <= to.radius; dz++) {
        for (int dx = -to.radius; dx <= to.radius; dx++) {
            int cx = to.center.x + dx;
            int cz = to.center.y + dz;
            if (!from.contains(cx, cz)) cols.push_back(glm::ivec2(cx, cz));
        }
    }
    std::sort(cols.begin(), cols.end(), [&to](const glm::ivec2& a, const glm::ivec2& b) {
        glm::ivec2 da = a - to.center, db = b - to.center;
        return da.x * da.x + da.y * da.y < db.x * db.x + db.y * db.y;
    });
    return cols;
}

// Called at startup, whenever the player enters a new chunk column, and when the render distance changes
void updateStreaming() {
    workerPlayerCx = playerChunk.x;
    workerPlayerCz = playerChunk.z;
    workerCancelRadius = std::max(R_ACTIVE + GPU_EVICT_MARGIN, R_CPU_KEEP);

    glm::ivec2 center(playerChunk.x, playerChunk.z);
    StreamWindow gpuLoad{ center, R_ACTIVE };
    StreamWindow gpuKeep{ center, R_ACTIVE + GPU_EVICT_MARGIN };
    StreamWindow cpuLoad{ center, R_CPU };
    StreamWindow cpuKeep{ center, R_CPU_KEEP };

    // 1. Evict GPU chunks that left the keep window
    for (const glm::ivec2& col : columnsEntering(gpuKeep, gpuKeepWindow)) {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) {
            uint64_t key = getChunkKey(col.x, cy, col.y);
            evictResident(key);
            knownEmptyChunks.erase(key);
        }
    }

    // 2. Free CPU chunks that left the keep window; edited chunks are kept forever
    for (const glm::ivec2& col : columnsEntering(cpuKeep, cpuKeepWindow)) {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) {
            auto it = worldMap.find(getChunkKey(col.x, cy, col.y));
            if (it != worldMap.end() && !it->second->isModified) {
                delete it->second;
                worldMap.erase(it);
            }
        }
    }

    // 3. Request CPU data for columns entering the CPU window (nearest first)
    for (const glm::ivec2& col : columnsEntering(cpuLoadWindow, cpuLoad)) {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) {
            if (!findChunk(col.x, cy, col.y)) requestChunk(col.x, cy, col.y);
        }
    }

    // 4. Upload or request chunks entering the render distance (nearest first)
    for (const glm::ivec2& col : columnsEntering(gpuLoadWindow, gpuLoad)) {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) {
            uint64_t key = getChunkKey(col.x, cy, col.y);
            if (chunkSlotMap.count(key) || knownEmptyChunks.count(key)) continue;
            if (Chunk* chunk = findChunk(col.x, cy, col.y)) {
                makeResident(key, chunk->position, chunk->data, nullptr);
            } else {
                requestChunk(col.x, cy, col.y);
            }
        }
    }

    gpuLoadWindow = gpuLoad;
    gpuKeepWindow = gpuKeep;
    cpuLoadWindow = cpuLoad;
    cpuKeepWindow = cpuKeep;
}

// Changes the render distance (in chunks) and immediately re-streams around the player
void setRenderDistance(int chunks) {
    chunks = std::clamp(chunks, MIN_RENDER_DISTANCE, MAX_RENDER_DISTANCE);
    if (chunks == R_ACTIVE) return;
    R_ACTIVE = chunks;
    updateStreaming();
}

// --- Collision ---

bool isPlayerColliding(glm::vec3 pos) {
    float r = PLAYER_RADIUS;

    // The world is unbounded horizontally; only keep the player inside the vertical slab
    if (pos.y < 0.0f || pos.y > (float)WORLD_HEIGHT / VOXELS_PER_UNIT) {
        return true;
    }

    // Check corners of bounding box
    for (int ix = -1; ix <= 1; ix += 2) {
        for (int iy = -1; iy <= 1; iy += 2) {
            for (int iz = -1; iz <= 1; iz += 2) {
                glm::vec3 p = pos + glm::vec3(ix * r, iy * r, iz * r);
                glm::ivec3 vox = glm::ivec3(glm::floor(p * VOXELS_PER_UNIT));
                if (isSolidBlock(getVoxelFromChunks(vox.x, vox.y, vox.z))) {
                    return true;
                }
            }
        }
    }
    return false;
}

void resolveCollisions(glm::vec3& pos, glm::vec3& velocity) {
    float r = PLAYER_RADIUS;

    // Do 4 iterations to handle multi-surface/corner collisions
    for (int iter = 0; iter < 4; iter++) {
        glm::vec3 minP = pos - glm::vec3(r);
        glm::vec3 maxP = pos + glm::vec3(r);

        glm::ivec3 minVox = glm::ivec3(glm::floor(minP * VOXELS_PER_UNIT));
        glm::ivec3 maxVox = glm::ivec3(glm::floor(maxP * VOXELS_PER_UNIT));

        bool collided = false;

        for (int vx = minVox.x; vx <= maxVox.x; vx++) {
            for (int vy = minVox.y; vy <= maxVox.y; vy++) {
                for (int vz = minVox.z; vz <= maxVox.z; vz++) {
                    if (isSolidBlock(getVoxelFromChunks(vx, vy, vz))) {
                        float voxMinX = (float)vx / VOXELS_PER_UNIT;
                        float voxMaxX = (float)(vx + 1) / VOXELS_PER_UNIT;
                        float voxMinY = (float)vy / VOXELS_PER_UNIT;
                        float voxMaxY = (float)(vy + 1) / VOXELS_PER_UNIT;
                        float voxMinZ = (float)vz / VOXELS_PER_UNIT;
                        float voxMaxZ = (float)(vz + 1) / VOXELS_PER_UNIT;

                        float overlapX = std::min(maxP.x, voxMaxX) - std::max(minP.x, voxMinX);
                        float overlapY = std::min(maxP.y, voxMaxY) - std::max(minP.y, voxMinY);
                        float overlapZ = std::min(maxP.z, voxMaxZ) - std::max(minP.z, voxMinZ);

                        if (overlapX > 0.0f && overlapY > 0.0f && overlapZ > 0.0f) {
                            collided = true;
                            // Push along the axis of minimum penetration
                            if (overlapX < overlapY && overlapX < overlapZ) {
                                float pushSign = (pos.x < (voxMinX + voxMaxX) * 0.5f) ? -1.0f : 1.0f;
                                pos.x += pushSign * overlapX;
                                velocity.x = 0.0f;
                            }
                            else if (overlapY < overlapX && overlapY < overlapZ) {
                                float pushSign = (pos.y < (voxMinY + voxMaxY) * 0.5f) ? -1.0f : 1.0f;
                                pos.y += pushSign * overlapY;
                                velocity.y = 0.0f;
                            }
                            else {
                                float pushSign = (pos.z < (voxMinZ + voxMaxZ) * 0.5f) ? -1.0f : 1.0f;
                                pos.z += pushSign * overlapZ;
                                velocity.z = 0.0f;
                            }
                            // Re-calculate bounds for subsequent checks in this iteration
                            minP = pos - glm::vec3(r);
                            maxP = pos + glm::vec3(r);
                        }
                    }
                }
            }
        }

        // Keep inside the vertical slab
        float topY = (float)WORLD_HEIGHT / VOXELS_PER_UNIT;
        if (pos.y < r) { pos.y = r; velocity.y = 0.0f; }
        if (pos.y > topY - r) { pos.y = topY - r; velocity.y = 0.0f; }

        if (!collided) break;
    }
}

bool playerOverlapsVoxel(glm::ivec3 v) {
    glm::vec3 minP = cameraPos - glm::vec3(PLAYER_RADIUS);
    glm::vec3 maxP = cameraPos + glm::vec3(PLAYER_RADIUS);
    glm::vec3 vMin = glm::vec3(v) / VOXELS_PER_UNIT;
    glm::vec3 vMax = glm::vec3(v + glm::ivec3(1)) / VOXELS_PER_UNIT;
    return glm::all(glm::lessThan(minP, vMax)) && glm::all(glm::greaterThan(maxP, vMin));
}

// --- World Editing ---

void modifyVoxelsInSphere(glm::ivec3 center, int radius, GLubyte val) {
    std::vector<uint64_t> touched;
    for (int dz = -radius; dz <= radius; dz++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx*dx + dy*dy + dz*dz > radius*radius) continue;
                int px = center.x + dx;
                int py = center.y + dy;
                int pz = center.z + dz;
                if (setVoxelInChunks(px, py, pz, val)) {
                    uint64_t key = getChunkKey(px >> 5, py >> 5, pz >> 5);
                    if (std::find(touched.begin(), touched.end(), key) == touched.end()) touched.push_back(key);
                }
            }
        }
    }
    // One upload per touched chunk instead of one per voxel
    for (uint64_t key : touched) refreshChunk(key);
    if (!touched.empty()) grassListDirty = true;
}

bool placeVoxel(glm::ivec3 pos, GLubyte id) {
    if (!setVoxelInChunks(pos.x, pos.y, pos.z, id)) return false;
    grassListDirty = true;
    refreshChunk(getChunkKey(pos.x >> 5, pos.y >> 5, pos.z >> 5));
    return true;
}

// --- Gameplay Systems ---

void updateProjectiles(float deltaTime) {
    for (auto& flare : activeFlares) {
        if (flare.life > 0.0f) flare.life -= deltaTime;
        if (flare.life < 2.0f) {
            flare.intensity = (flare.life / 2.0f) * 1.2f;
        }

        if (!flare.isStuck) {
            flare.vel.y -= 0.04f * REFERENCE_FPS * deltaTime; // gravity
            glm::vec3 step = flare.vel * deltaTime;
            float len = glm::length(step);
            if (len > 0.0f) {
                // Swept test so fast flares cannot pass through thin walls
                RayHit hit = raycast(flare.pos, step / len, len);
                if (hit.hit) {
                    // Rest on the face that was hit so the light is not inside the block
                    flare.pos = (glm::vec3(hit.mapPos) + 0.5f + glm::vec3(hit.normal) * 0.6f) / VOXELS_PER_UNIT;
                    flare.isStuck = true;
                    flare.vel = glm::vec3(0.0f);
                } else {
                    flare.pos += step;
                }
            }
        }
    }
    activeFlares.erase(std::remove_if(activeFlares.begin(), activeFlares.end(), [](const FlareProjectile& f) {
        return f.life <= 0.0f;
    }), activeFlares.end());

    for (auto& proj : activeClumpProjectiles) {
        if (!proj.active) continue;

        proj.vel.y -= 0.08f * REFERENCE_FPS * deltaTime; // gravity
        glm::vec3 step = proj.vel * deltaTime;
        float len = glm::length(step);
        if (len > 0.0f) {
            RayHit hit = raycast(proj.pos, step / len, len);
            if (hit.hit) {
                modifyVoxelsInSphere(hit.mapPos, 4, 0); // explode
                proj.active = false;
                continue;
            }
            proj.pos += step;
        }

        float voxY = proj.pos.y * VOXELS_PER_UNIT;
        if (voxY < 0.0f || voxY >= (float)WORLD_HEIGHT) {
            proj.active = false;
        }
    }
    activeClumpProjectiles.erase(std::remove_if(activeClumpProjectiles.begin(), activeClumpProjectiles.end(), [](const ClumpProjectile& p) {
        return !p.active;
    }), activeClumpProjectiles.end());
}

void updateScanner(float deltaTime) {
    static float scanTimer = 0.0f;
    scanTimer += deltaTime;
    if (scanTimer < 0.2f) return;
    scanTimer = 0.0f;

    float minDist = 1e9f;
    for (const auto& pair : worldMap) {
        Chunk* chunk = pair.second;
        if (chunk->artifactIdx.empty()) continue;

        glm::ivec3 base = chunk->position * CHUNK_SIZE;
        glm::vec3 chunkCenter = glm::vec3(base + glm::ivec3(16)) / VOXELS_PER_UNIT;
        if (glm::distance(cameraPos, chunkCenter) > 0.3f) continue;

        // Drop entries whose artifact was dug out or picked up
        auto& list = chunk->artifactIdx;
        list.erase(std::remove_if(list.begin(), list.end(), [chunk](uint16_t i) {
            return chunk->data[i] != 32;
        }), list.end());

        for (uint16_t i : list) {
            int lx = i % 32;
            int ly = (i / 32) % 32;
            int lz = i / 1024;
            glm::vec3 artPos = glm::vec3(base + glm::ivec3(lx, ly, lz)) / VOXELS_PER_UNIT;
            minDist = std::min(minDist, glm::distance(cameraPos, artPos));
        }
    }
    if (minDist < 1e8f) {
        closestArtifactDistance = minDist * VOXELS_PER_UNIT;
    } else {
        closestArtifactDistance = -1.0f;
    }
}

void generateMinimap(GLuint tex) {
    glm::ivec3 p = glm::ivec3(glm::floor(cameraPos * VOXELS_PER_UNIT));

    int start_x = p.x - 32;
    int start_z = p.z - 32;
    int checkY = p.y - 2;

    for (int mz = 0; mz < 64; mz++) {
        for (int mx = 0; mx < 64; mx++) {
            GLubyte val = getVoxelFromChunks(start_x + mx, checkY, start_z + mz);

            int idx = (mz * 64 + mx) * 4;
            GLubyte r = 15, g = 15, b = 20, a = 255; // Background
            if (val == 1) { r = 34; g = 139; b = 34; }    // Grass
            else if (val == 2) { r = 139; g = 69; b = 19; } // Dirt
            else if (val == 3) { r = 128; g = 128; b = 128; } // Stone
            else if (val == 4) { r = 218; g = 165; b = 32; }  // Sand
            else if (val == 30) { r = 0; g = 255; b = 200; } // Cavern glow
            else if (val == 32) { r = 255; g = 0; b = 255; } // Artifact
            else if (val == WATER_ID) { r = 30; g = 110; b = 200; } // Water

            if (mx >= 30 && mx <= 33 && mz >= 30 && mz <= 33) {
                r = 255; g = 255; b = 255; // Player dot
            }

            minimapData[idx] = r;
            minimapData[idx+1] = g;
            minimapData[idx+2] = b;
            minimapData[idx+3] = a;
        }
    }

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, minimapData.data());
}


// --- Animated Grass ---
// Every grass tuft in the world is baked into chunk data in its rest pose by the generator.
// Tufts within R_GRASS_ANIM chunks of the player are also handed to anim.comp, which bends
// them with the wind on the GPU. Tufts that leave the radius are "retired": the GPU puts
// them back in the rest pose, and the next rebuild drops them.
const int R_GRASS_ANIM = 4;
const glm::vec2 WIND_DIR = glm::normalize(glm::vec2(1.0f, 0.35f));
const int GRASS_REST_POSE = 8 | (8 << 4); // encodePose(0, 0) in anim.comp
const double GRASS_REBUILD_INTERVAL = 0.25; // Seconds between rebuilds caused by edits
GLuint plantSSBO = 0;
double lastGrassRebuild = -1.0;
TerrainGen* grassPlacement = nullptr; // Main-thread generator copy, for tuft positions

inline uint64_t grassColumnKey(int wx, int wz) {
    return ((uint64_t)(uint32_t)wx << 32) | (uint32_t)wz;
}

// Rebuilds the list of animated tufts around the player. Reads the current poses back from
// the GPU first, so tufts that stay in range keep animating from where they are.
void rebuildAnimatedGrass() {
    std::unordered_map<uint64_t, AnimatedPlant> previous;
    if (!activePlants.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, plantSSBO);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, activePlants.size() * sizeof(AnimatedPlant), activePlants.data());
        for (const AnimatedPlant& plant : activePlants) {
            if (plant.params.z != 0.0f) continue; // Retired last time; already back at rest
            previous[grassColumnKey(plant.posAndPose.x, plant.posAndPose.z)] = plant;
        }
    }

    std::vector<AnimatedPlant> next;
    int x0 = (playerChunk.x - R_GRASS_ANIM) * CHUNK_SIZE, x1 = (playerChunk.x + R_GRASS_ANIM + 1) * CHUNK_SIZE - 1;
    int z0 = (playerChunk.z - R_GRASS_ANIM) * CHUNK_SIZE, z1 = (playerChunk.z + R_GRASS_ANIM + 1) * CHUNK_SIZE - 1;
    for (int gz = z0 >> 2; gz <= z1 >> 2; gz++) {
        for (int gx = x0 >> 2; gx <= x1 >> 2; gx++) {
            GrassTuft tuft;
            if (!grassPlacement->grassTuftInCell(gx, gz, tuft)) continue;

            // Skip tufts whose grass block was dug out (only checkable where CPU data is loaded)
            glm::ivec3 below = tuft.root - glm::ivec3(0, 1, 0);
            if (findChunk(below.x >> 5, below.y >> 5, below.z >> 5) && getVoxelFromChunks(below.x, below.y, below.z) != 1) continue;

            AnimatedPlant plant;
            plant.posAndPose = glm::ivec4(tuft.root, GRASS_REST_POSE); // Rest pose is already in the chunk data
            plant.params = glm::vec4(tuft.phase, (float)tuft.variant, 0.0f, 0.0f);
            auto it = previous.find(grassColumnKey(tuft.root.x, tuft.root.z));
            if (it != previous.end()) {
                plant.posAndPose.w = it->second.posAndPose.w;
                previous.erase(it);
            }
            next.push_back(plant);
        }
    }

    // Tufts that left the radius go back to the rest pose on the GPU
    for (auto& entry : previous) {
        AnimatedPlant plant = entry.second;
        if (plant.posAndPose.w == GRASS_REST_POSE) continue;
        plant.params.z = 1.0f; // Retire
        next.push_back(plant);
    }

    activePlants = std::move(next);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, plantSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, activePlants.size() * sizeof(AnimatedPlant), activePlants.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, plantSSBO);
    grassListDirty = false;
    lastGrassRebuild = glfwGetTime();
}

// --- Spawn ---

// Nearest column (searched in rings from SPAWN_SEARCH_START) that is solid land with land
// 24 voxels around it too, so the player does not start on a sliver of beach.
glm::ivec2 FindSpawnColumn(TerrainGen& terrain) {
    auto isLand = [&terrain](int wx, int wz) {
        int biome = terrain.biomeAt(terrain.biomeNoise.GetNoise((float)wx, (float)wz));
        return terrain.terrainHeightAt(wx, wz, biome) >= SEA_LEVEL + ISLAND_HEIGHT;
    };
    const int STEP = 16, PROBE = 24, MAX_RINGS = 400;
    for (int ring = 0; ring <= MAX_RINGS; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue; // Ring edge only
                int x = SPAWN_SEARCH_START.x + dx * STEP, z = SPAWN_SEARCH_START.y + dz * STEP;
                if (isLand(x, z) && isLand(x + PROBE, z) && isLand(x - PROBE, z) && isLand(x, z + PROBE) && isLand(x, z - PROBE)) {
                    return glm::ivec2(x, z);
                }
            }
        }
    }
    return SPAWN_SEARCH_START; // Nothing found; spawn over the ocean
}

// --- Callbacks & Input ---

void launchHeldClump() {
    ClumpProjectile proj;
    proj.pos = heldClumpPos;
    proj.vel = cameraFront * 0.5f * REFERENCE_FPS;
    proj.radius = heldClumpRadius;
    proj.active = true;
    activeClumpProjectiles.push_back(proj);
    heldClumpActive = false;
    heldClumpIsArtifact = false;
    laserBeamActive = false;
}

void checkMouseInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
        laserBeamActive = false;
        return;
    }

    // --- Matter Dissolver (Left Click) ---
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        RayHit hit = raycast(cameraPos, cameraFront, REACH_DISTANCE);
        if (hit.hit) {
            glm::vec3 hitWorldPos = glm::vec3(hit.mapPos) / VOXELS_PER_UNIT;

            // Continuous deletion in a sphere of radius 2
            modifyVoxelsInSphere(hit.mapPos, 2, 0);

            // Draw Dissolver Beam (Red)
            laserBeamActive = true;
            glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
            laserBeamStart = cameraPos + cameraFront * 0.1f + right * 0.03f + cameraUp * -0.02f;
            laserBeamEnd = hitWorldPos;
            laserBeamColor = glm::vec3(1.0f, 0.2f, 0.1f);
        } else {
            laserBeamActive = false;
        }
    } else {
        if (!heldClumpActive) {
            laserBeamActive = false;
        }
    }

    static bool rightWasPressed = false;
    bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    // --- Block Placement (Right Click in Paint Mode) ---
    if (paintModeEnabled && !heldClumpActive) {
        if (rightPressed && !rightWasPressed) {
            RayHit hit = raycast(cameraPos, cameraFront, REACH_DISTANCE);
            if (hit.hit && hit.normal != glm::ivec3(0)) {
                glm::ivec3 target = hit.mapPos + hit.normal;
                if (!playerOverlapsVoxel(target)) {
                    placeVoxel(target, (GLubyte)selectedBlockID);
                }
            }
        }
        rightWasPressed = rightPressed;
        return;
    }

    // --- Gravity Tether (Right Click) ---
    if (rightPressed) {
        if (!heldClumpActive) {
            if (!rightWasPressed) {
                RayHit hit = raycast(cameraPos, cameraFront, REACH_DISTANCE);
                if (hit.hit) {
                    GLubyte val = getVoxelFromChunks(hit.mapPos.x, hit.mapPos.y, hit.mapPos.z);
                    if (val > 0) {
                        heldClumpActive = true;
                        heldClumpIsArtifact = (val == 32);
                        heldClumpPos = glm::vec3(hit.mapPos) / VOXELS_PER_UNIT;

                        modifyVoxelsInSphere(hit.mapPos, 2, 0);
                    }
                }
            }
        } else {
            glm::vec3 targetPos = cameraPos + cameraFront * 0.05f + glm::vec3(0.0f, -0.01f, 0.0f);
            float follow = 1.0f - std::pow(0.9f, deltaTime * REFERENCE_FPS);
            heldClumpPos = glm::mix(heldClumpPos, targetPos, follow);

            laserBeamActive = true;
            glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
            laserBeamStart = cameraPos + cameraFront * 0.1f + right * -0.03f + cameraUp * -0.02f;
            laserBeamEnd = heldClumpPos;
            laserBeamColor = glm::vec3(0.1f, 0.5f, 1.0f);

            if (heldClumpIsArtifact) {
                glm::vec2 beacon = glm::vec2(spawnVoxel.x, spawnVoxel.z) / VOXELS_PER_UNIT;
                float distToBeacon = glm::distance(glm::vec2(heldClumpPos.x, heldClumpPos.z), beacon);
                if (distToBeacon < 0.03f) {
                    playerArtifactsRetrieved++;
                    heldClumpActive = false;
                    heldClumpIsArtifact = false;
                    laserBeamActive = false;
                }
            }
        }
    } else if (heldClumpActive) {
        launchHeldClump();
    }
    rightWasPressed = rightPressed;
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
        firstMouse = true;
        return;
    }
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Constrain pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// Edge-triggered key toggle
void toggleOnPress(GLFWwindow* window, int key, bool& wasPressed, bool& flag) {
    if (glfwGetKey(window, key) == GLFW_PRESS) {
        if (!wasPressed) {
            flag = !flag;
            wasPressed = true;
        }
    } else {
        wasPressed = false;
    }
}

void processInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Debounced hotkey triggers
    static bool f3WasPressed = false;
    static bool pWasPressed = false;
    static bool cWasPressed = false;
    static bool lWasPressed = false;
    static bool tabWasPressed = false;

    // Toggle cursor mode (Tab)
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (!tabWasPressed) {
            int currentMode = glfwGetInputMode(window, GLFW_CURSOR);
            if (currentMode == GLFW_CURSOR_DISABLED) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            tabWasPressed = true;
        }
    } else {
        tabWasPressed = false;
    }

    toggleOnPress(window, GLFW_KEY_F3, f3WasPressed, debugOverlayEnabled); // Debug Overlay
    toggleOnPress(window, GLFW_KEY_P, pWasPressed, paintModeEnabled);      // Paint Mode
    toggleOnPress(window, GLFW_KEY_C, cWasPressed, chunkViewerEnabled);    // Chunk Viewer
    toggleOnPress(window, GLFW_KEY_L, lWasPressed, lightVisualizerEnabled); // Lighting Visualizer

    // Launch dynamic colored flares (Key: F)
    static bool fWasPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!fWasPressed && activeFlares.size() < 8) {
            FlareProjectile flare;
            flare.pos = cameraPos;
            flare.vel = cameraFront * 0.015f * REFERENCE_FPS; // launch velocity in world units / second
            static int colorIdx = 0;
            glm::vec3 colors[] = {
                glm::vec3(1.0f, 0.4f, 0.1f), // orange
                glm::vec3(0.1f, 0.8f, 1.0f), // cyan
                glm::vec3(0.8f, 0.1f, 1.0f)  // purple
            };
            flare.color = colors[colorIdx % 3];
            colorIdx++;
            flare.intensity = 1.5f;
            flare.isStuck = false;
            flare.life = 30.0f;
            activeFlares.push_back(flare);
            fWasPressed = true;
        }
    } else {
        fWasPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) selectedBlockID = 1;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) selectedBlockID = 2;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) selectedBlockID = 3;
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) selectedBlockID = 4;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) selectedBlockID = 5;

    // Render distance (Page Up / Page Down), one chunk per press
    static bool pageUpWasPressed = false;
    static bool pageDownWasPressed = false;
    bool pageUp = glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS;
    bool pageDown = glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS;
    if (pageUp && !pageUpWasPressed) setRenderDistance(R_ACTIVE + 1);
    if (pageDown && !pageDownWasPressed) setRenderDistance(R_ACTIVE - 1);
    pageUpWasPressed = pageUp;
    pageDownWasPressed = pageDown;

    // Physics-based Drone movement controls
    glm::vec3 accelDir = glm::vec3(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) accelDir += cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) accelDir -= cameraFront;

    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) accelDir -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) accelDir += right;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) accelDir += cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) accelDir -= cameraUp;

    // Normalizing acceleration direction
    if (glm::length(accelDir) > 0.0001f) {
        accelDir = glm::normalize(accelDir);
    }

    // Drone flight parameters
    float accelRate = 0.8f;  // acceleration speed (world units/s^2)
    float dragRate = 3.5f;   // drag/resistance (high values = faster braking)
    float maxSpeed = 0.05f;  // normal max velocity

    // Boost multiplier
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        accelRate *= 5.0f;
        maxSpeed *= 4.0f;
    }

    // Apply acceleration and drag
    playerVelocity += accelDir * accelRate * deltaTime;
    playerVelocity -= playerVelocity * dragRate * deltaTime;

    // Cap velocity to maxSpeed
    float currentSpeed = glm::length(playerVelocity);
    if (currentSpeed > maxSpeed) {
        playerVelocity = glm::normalize(playerVelocity) * maxSpeed;
    }

    // Sliding collision checks (axis-aligned check & update), sub-stepped so no step
    // moves more than half a voxel and the drone cannot tunnel through walls
    if (currentSpeed > 0.0001f) {
        glm::vec3 move = playerVelocity * deltaTime;
        float maxComponent = std::max({ std::abs(move.x), std::abs(move.y), std::abs(move.z) });
        int subSteps = std::max(1, (int)std::ceil(maxComponent * VOXELS_PER_UNIT / 0.5f));

        for (int s = 0; s < subSteps; s++) {
            float stepDt = deltaTime / subSteps;
            for (int axis = 0; axis < 3; axis++) {
                glm::vec3 next = cameraPos;
                next[axis] += playerVelocity[axis] * stepDt;
                if (!isPlayerColliding(next)) {
                    cameraPos[axis] = next[axis];
                } else {
                    playerVelocity[axis] *= -0.2f; // Slight bounce
                }
            }
        }
    }

    // Always run collision resolution to push player out of walls if they penetrate slightly
    resolveCollisions(cameraPos, playerVelocity);
}

// --- Shaders ---

bool checkShaderErrors(GLuint shader, const std::string& type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
        }
    }
    return success != 0;
}

// Returns 0 on failure (error already printed). passDefine, if given, is inserted as a
// #define right after the #version line, to build one pass of a multi-pass shader.
GLuint loadComputeProgram(const char* path, const char* passDefine = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open shader file: " << path << std::endl;
        return 0;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();
    if (passDefine) {
        size_t versionEnd = source.find('\n');
        source.insert(versionEnd == std::string::npos ? source.size() : versionEnd + 1,
            std::string("#define ") + passDefine + "\n");
    }
    const char* src = source.c_str();

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    bool ok = checkShaderErrors(shader, std::string("COMPUTE (") + path + (passDefine ? std::string(", ") + passDefine : "") + ")");

    GLuint program = 0;
    if (ok) {
        program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);
        if (!checkShaderErrors(program, "PROGRAM")) {
            glDeleteProgram(program);
            program = 0;
        }
    }
    glDeleteShader(shader);
    return program;
}

struct ComputeUniforms {
    GLint pageTable, poolBase, chunkViewerEnabled, lightVisualizerEnabled;
    GLint laserBeamActive, laserBeamStart, laserBeamEnd, laserBeamColor;
    GLint numFlares, flarePos[8], flareColor[8], flareIntensity[8];
    GLint halfResShadows, renderSize, heldClumpActive, heldClumpPos, heldClumpRadius, heldClumpIsArtifact;
    GLint seaLevel, beaconPos, oceanTileSizes, oceanChoppiness, shoreOrigin, renderDistanceVoxels, dimX, dimY, dimZ, cameraPos, inverseView, inverseProj, time;
    GLint sunDir, moonDir, lightDir, lightColor, skyColor, ambient;

    ComputeUniforms() = default;

    explicit ComputeUniforms(GLuint p) {
        auto loc = [p](const char* name) { return glGetUniformLocation(p, name); };
        pageTable = loc("pageTable");
        poolBase = loc("poolBase");
        chunkViewerEnabled = loc("chunkViewerEnabled");
        lightVisualizerEnabled = loc("lightVisualizerEnabled");
        laserBeamActive = loc("laserBeamActive");
        laserBeamStart = loc("laserBeamStart");
        laserBeamEnd = loc("laserBeamEnd");
        laserBeamColor = loc("laserBeamColor");
        numFlares = loc("numFlares");
        for (int i = 0; i < 8; i++) {
            std::string base = "flares[" + std::to_string(i) + "].";
            flarePos[i] = loc((base + "pos").c_str());
            flareColor[i] = loc((base + "color").c_str());
            flareIntensity[i] = loc((base + "intensity").c_str());
        }
        halfResShadows = loc("halfResShadows");
        renderSize = loc("renderSize");
        heldClumpActive = loc("heldClumpActive");
        heldClumpPos = loc("heldClumpPos");
        heldClumpRadius = loc("heldClumpRadius");
        heldClumpIsArtifact = loc("heldClumpIsArtifact");
        renderDistanceVoxels = loc("renderDistanceVoxels");
        seaLevel = loc("seaLevel");
        oceanTileSizes = loc("oceanTileSizes");
        oceanChoppiness = loc("oceanChoppiness");
        shoreOrigin = loc("shoreOrigin");
        beaconPos = loc("beaconPos");
        dimX = loc("dimX");
        dimY = loc("dimY");
        dimZ = loc("dimZ");
        cameraPos = loc("cameraPos");
        inverseView = loc("inverseView");
        inverseProj = loc("inverseProj");
        time = loc("time");
        sunDir = loc("sunDir");
        moonDir = loc("moonDir");
        lightDir = loc("lightDir");
        lightColor = loc("lightColor");
        skyColor = loc("skyColor");
        ambient = loc("ambient");
    }
};

struct AnimUniforms {
    GLint time, numPlants, pageTable, poolBase, mode, windDir;
    explicit AnimUniforms(GLuint p) {
        time = glGetUniformLocation(p, "time");
        numPlants = glGetUniformLocation(p, "numPlants");
        pageTable = glGetUniformLocation(p, "pageTable");
        poolBase = glGetUniformLocation(p, "poolBase");
        mode = glGetUniformLocation(p, "mode");
        windDir = glGetUniformLocation(p, "windDir");
    }
};

struct ComputePass {
    GLuint program = 0;
    ComputeUniforms uniforms;
};

// Render targets at the render resolution (window size * renderScale)
struct RenderTargets {
    GLuint color = 0;       // RGBA32F final image, blitted to the window
    GLuint gBuffer = 0;     // RGBA32I: hit voxel xyz + packed normal/voxel ID/grid factor
    GLuint shadowValue = 0; // R32F, half resolution
    GLuint shadowKey = 0;   // RGBA32I, half resolution: voxel + normal each shadow was computed for
    int width = 0, height = 0;
};

void allocateTexture2D(GLuint& tex, GLenum internalFormat, int width, int height, GLenum filter) {
    if (tex) glDeleteTextures(1, &tex);
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
}

// (Re)allocates every render target and attaches the color image to the blit FBO
void allocateRenderTargets(RenderTargets& rt, GLuint fbo, int width, int height) {
    rt.width = width;
    rt.height = height;
    int halfWidth = (width + 1) / 2, halfHeight = (height + 1) / 2;
    allocateTexture2D(rt.color, GL_RGBA32F, width, height, GL_LINEAR);
    allocateTexture2D(rt.gBuffer, GL_RGBA32I, width, height, GL_NEAREST);
    allocateTexture2D(rt.shadowValue, GL_R32F, halfWidth, halfHeight, GL_NEAREST);
    allocateTexture2D(rt.shadowKey, GL_RGBA32I, halfWidth, halfHeight, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.color, 0);
}

double renderTargetBytes(const RenderTargets& rt) {
    double full = (double)rt.width * rt.height;
    double half = (double)((rt.width + 1) / 2) * ((rt.height + 1) / 2);
    return full * (16.0 + 16.0) + half * (4.0 + 16.0);
}

ImU32 hudColor(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

// Crosshair and block hotbar, drawn by ImGui at full window resolution so they stay sharp at
// any render scale. Same layout as the old in-shader version (pixel units, hotbar 16 px above
// the bottom edge).
void drawHud(glm::vec3 crosshairColor) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float cx = std::floor(display.x * 0.5f);
    float cy = std::floor(display.y * 0.5f);
    auto rect = [dl](float x0, float y0, float x1, float y1, ImU32 col) {
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col);
    };

    // Crosshair: black outline, then the scan-colored lines (center pixel left open)
    ImU32 black = hudColor(0.0f, 0.0f, 0.0f);
    rect(cx - 9, cy - 1, cx, cy + 2, black);
    rect(cx + 1, cy - 1, cx + 10, cy + 2, black);
    rect(cx - 1, cy - 9, cx + 2, cy, black);
    rect(cx - 1, cy + 1, cx + 2, cy + 10, black);
    ImU32 inner = hudColor(crosshairColor.r, crosshairColor.g, crosshairColor.b);
    rect(cx - 8, cy, cx - 1, cy + 1, inner);
    rect(cx + 2, cy, cx + 9, cy + 1, inner);
    rect(cx, cy - 8, cx + 1, cy - 1, inner);
    rect(cx, cy + 2, cx + 1, cy + 9, inner);

    // Hotbar: 5 cells of 40 px
    const int CELLS = 5;
    const float CELL = 40.0f;
    float x0 = cx - 102.0f;
    float y0 = display.y - 56.0f;
    float width = 204.0f, height = 40.0f;

    rect(x0, y0, x0 + width, y0 + height, hudColor(0.15f, 0.15f, 0.15f, 0.85f)); // Panel

    ImU32 grey = hudColor(0.3f, 0.3f, 0.3f);
    rect(x0, y0, x0 + width, y0 + 3, grey);                   // Frame top
    rect(x0, y0 + height - 3, x0 + width, y0 + height, grey); // Frame bottom
    rect(x0 + width - 3, y0, x0 + width, y0 + height, grey);  // Frame right
    for (int i = 0; i < CELLS; i++) {                         // Cell separators
        float cellX = x0 + i * CELL;
        rect(cellX, y0, cellX + 3, y0 + height, grey);
        rect(cellX + 37, y0, cellX + 40, y0 + height, grey);
    }

    const glm::vec3 previews[CELLS] = {
        glm::vec3(0.035f, 0.731f, 0.000f), // Grass
        glm::vec3(0.180f, 0.149f, 0.012f), // Dirt (using color ID 23)
        glm::vec3(0.5f, 0.5f, 0.5f),       // Stone
        glm::vec3(0.85f, 0.75f, 0.45f),    // Sand
        glm::vec3(0.137f, 0.306f, 0.016f), // Plant
    };
    for (int i = 0; i < CELLS; i++) {
        float cellX = x0 + i * CELL;
        rect(cellX + 8, y0 + 8, cellX + 32, y0 + 32, hudColor(previews[i].r, previews[i].g, previews[i].b));
    }

    // Thick white border around the selected slot
    int active = selectedBlockID - 1;
    if (active >= 0 && active < CELLS) {
        ImU32 white = hudColor(1.0f, 1.0f, 1.0f);
        float cellX = x0 + active * CELL;
        rect(cellX + 1, y0 + 1, cellX + 39, y0 + 5, white);
        rect(cellX + 1, y0 + 35, cellX + 39, y0 + 39, white);
        rect(cellX + 1, y0 + 1, cellX + 5, y0 + 39, white);
        rect(cellX + 35, y0 + 1, cellX + 39, y0 + 39, white);
    }
}

int main(int argc, char** argv) {
    // Optional launch settings:
    //   --render-distance N   starting render distance in chunks
    //   --render-scale F      fraction of the window resolution to trace (0.5 - 1.0)
    for (int i = 1; i + 1 < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--render-distance") {
            R_ACTIVE = std::clamp(std::atoi(argv[i + 1]), MIN_RENDER_DISTANCE, MAX_RENDER_DISTANCE);
        } else if (arg == "--render-scale") {
            renderScale = std::clamp((float)std::atof(argv[i + 1]), MIN_RENDER_SCALE, 1.0f);
        }
    }

    // 1. Initialize GLFW
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL Compute DDA Voxel", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    // --- Input Setup ---
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 2. Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    // 3. Load shaders first so a missing or broken file fails fast
    ComputePass tracePass, shadowPass, shadePass;
    tracePass.program = loadComputeProgram("default.comp", "PASS_TRACE");
    shadowPass.program = loadComputeProgram("default.comp", "PASS_SHADOW");
    shadePass.program = loadComputeProgram("default.comp", "PASS_SHADE");
    GLuint animProgram = loadComputeProgram("anim.comp");
    GLuint oceanSpectrumProgram = loadComputeProgram("ocean.comp", "PASS_SPECTRUM");
    GLuint oceanFftProgram = loadComputeProgram("ocean.comp", "PASS_FFT");
    GLuint shoreSeedProgram = loadComputeProgram("default.comp", "PASS_SHORE_SEED");
    GLuint shoreJumpFloodProgram = loadComputeProgram("default.comp", "PASS_SHORE_JFA");
    GLuint shoreFinalProgram = loadComputeProgram("default.comp", "PASS_SHORE_FINAL");
    if (!tracePass.program || !shadowPass.program || !shadePass.program || !animProgram || !oceanSpectrumProgram || !oceanFftProgram ||
        !shoreSeedProgram || !shoreJumpFloodProgram || !shoreFinalProgram) {
        glfwTerminate();
        return -1;
    }
    for (ComputePass* pass : { &tracePass, &shadowPass, &shadePass }) {
        pass->uniforms = ComputeUniforms(pass->program);
    }
    AnimUniforms au(animProgram);

    OceanSimulation ocean;
    ocean.Init(oceanSpectrumProgram, oceanFftProgram);
    const float OCEAN_CHOPPINESS = 1.6f; // Horizontal displacement strength, used for foam

    ShoreMap shore;
    shore.Init(shoreSeedProgram, shoreJumpFloodProgram, shoreFinalProgram);

    // 4. Render targets (allocated in the loop once the size is known) and the FBO used to blit
    int fbWidth = 0, fbHeight = 0;
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    RenderTargets targets;

    // 5. Create GPU Virtual Textures (Page Table & first Chunk Pool)
    glGenTextures(1, &pageTableTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, pageTableTex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // Toroidal 512 x 4 x 512 page table texture, storing [slotIndex+1, cx, cy, cz] using signed 32-bit integers
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32I, PAGE_TABLE_WRAP, CHUNK_LAYERS, PAGE_TABLE_WRAP, 0, GL_RGBA_INTEGER, GL_INT, pageTableData.data());

    // Further pools are added on demand as the render distance grows
    if (!addChunkPool()) {
        std::cerr << "Failed to allocate the chunk pool." << std::endl;
        glfwTerminate();
        return -1;
    }

    const double pageTableBytes = (double)pageTableData.size() * sizeof(glm::ivec4);
    const double minimapBytes = 64.0 * 64.0 * 4.0;

    // Initialize Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.85f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.4f, 0.8f, 0.6f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.5f, 1.0f, 0.8f);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 440");

    // Initialize Minimap Texture
    glGenTextures(1, &minimapTex);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, minimapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 6. Load the tree model (must happen before workers start reading it)
    treeOffsets = loadVoxToOffsets(treeModelPath);
    if (treeOffsets.empty()) {
        std::cerr << "Tree model missing or empty; terrain will have no trees." << std::endl;
    } else {
        treeMin = treeMax = glm::ivec3(treeOffsets[0].x, treeOffsets[0].y, treeOffsets[0].z);
        for (const auto& off : treeOffsets) {
            treeMin = glm::min(treeMin, glm::ivec3(off.x, off.y, off.z));
            treeMax = glm::max(treeMax, glm::ivec3(off.x, off.y, off.z));
        }
    }

    // 7. Start background terrain generation threads
    unsigned int hwThreads = std::thread::hardware_concurrency(); // May report 0
    unsigned int workerCount = hwThreads > 1 ? hwThreads - 1 : 1;
    workerRunning = true;
    for (unsigned int i = 0; i < workerCount; i++) {
        workerThreads.emplace_back(chunkGeneratorWorker);
    }

    // 8. Start streaming around spawn, and wait only for the spawn column itself
    TerrainGen mainThreadTerrain; // Main-thread generator copy (spawn search, grass placement)
    glm::ivec2 spawnColumn = FindSpawnColumn(mainThreadTerrain);
    spawnVoxel = glm::ivec3(spawnColumn.x, 0, spawnColumn.y);
    playerChunk = glm::ivec3(spawnVoxel.x >> 5, 0, spawnVoxel.z >> 5);
    updateStreaming(); // Requests are nearest-first, so the spawn column arrives first
    auto spawnColumnReady = [&]() {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) {
            if (!findChunk(playerChunk.x, cy, playerChunk.z)) return false;
        }
        return true;
    };
    while (!spawnColumnReady()) {
        ChunkResult res;
        resultQueue.wait_and_pop(res);
        integrateResult(res);
    }
    {
        int spawnY = WORLD_HEIGHT - 1;
        // Find the highest solid block at spawn position
        while (spawnY > 0 && !isSolidBlock(getVoxelFromChunks(spawnVoxel.x, spawnY, spawnVoxel.z))) {
            spawnY--;
        }
        // Place camera 3 voxels above the surface
        cameraPos = glm::vec3((float)spawnVoxel.x, (float)(spawnY + 3), (float)spawnVoxel.z) / VOXELS_PER_UNIT;
    }

    glGenBuffers(1, &plantSSBO); // Filled by rebuildAnimatedGrass, bound to SSBO slot 3
    grassPlacement = &mainThreadTerrain;

    // 9. Loop
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double frameStartTime = glfwGetTime();

        // Clamp so a stall (window drag, breakpoint) can't launch the player through walls
        float deltaTime = (float)std::min(frameStartTime - lastTime, 0.05);
        lastTime = frameStartTime;

        // Handle window resize / minimise / render scale changes
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        bool minimized = (fbWidth == 0 || fbHeight == 0);
        if (!minimized) {
            int targetWidth = std::max(1, (int)std::lround(fbWidth * renderScale));
            int targetHeight = std::max(1, (int)std::lround(fbHeight * renderScale));
            if (targetWidth != targets.width || targetHeight != targets.height) {
                allocateRenderTargets(targets, fbo, targetWidth, targetHeight);
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Dynamic Chunk Streaming (Sliding Window) ---
        glm::ivec3 newPlayerChunk = glm::ivec3(glm::floor(cameraPos * VOXELS_PER_UNIT / (float)CHUNK_SIZE));
        bool crossedChunk = newPlayerChunk.x != playerChunk.x || newPlayerChunk.z != playerChunk.z;
        playerChunk = newPlayerChunk;

        // Process completed chunk requests within a per-frame time budget to avoid upload hitches
        const double STREAMING_BUDGET_SECONDS = 0.004;
        ChunkResult res;
        while (glfwGetTime() - frameStartTime < STREAMING_BUDGET_SECONDS && resultQueue.pop(res)) {
            integrateResult(res);
        }

        // Only update the streaming windows when the player crosses a chunk boundary
        if (crossedChunk) {
            updateStreaming();
        }
        if (crossedChunk) grassListDirty = true;
        if (GRASS_TUFTS_ENABLED && grassListDirty && glfwGetTime() - lastGrassRebuild >= GRASS_REBUILD_INTERVAL) rebuildAnimatedGrass();

        // Process Input
        processInput(window, deltaTime);

        // Update Project Strata systems
        updateProjectiles(deltaTime);
        updateScanner(deltaTime);
        generateMinimap(minimapTex);

        // Update mouse inputs (Matter Dissolver / Gravity Tether / Placement)
        checkMouseInput(window, deltaTime);

        // --- Render Dear ImGui UI ---
        if (debugOverlayEnabled) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(400, 560), ImGuiCond_FirstUseEver);
            ImGui::Begin("Project Strata: Vertical Slice", &debugOverlayEnabled);

            // Display gameplay metrics
            ImGui::Text("--- MISSION OBJECTIVES ---");
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Artifacts Retrieved: %d", playerArtifactsRetrieved);
            if (closestArtifactDistance >= 0.0f) {
                ImGui::Text("Scanner: %.1f m [Signal Strong]", closestArtifactDistance);
            } else {
                ImGui::Text("Scanner: No Signal...");
            }
            ImGui::Text("Carrying Artifact: %s", heldClumpIsArtifact ? "YES (Deliver to green beacon at spawn)" : "NO");

            ImGui::Separator();

            // Display performance metrics
            double poolBytes = 0.0;
            int totalSlots = 0;
            for (int p = 0; p < poolCount; p++) {
                totalSlots += pools[p].layers * SLOTS_PER_LAYER;
                poolBytes += pools[p].layers * SLOTS_PER_LAYER * (32768.0 + 64.0); // voxels + brick mask
            }
            double vramMB = (poolBytes + pageTableBytes + minimapBytes + renderTargetBytes(targets)) / (1024.0 * 1024.0);
            ImGui::Text("--- ENGINE METRICS ---");
            ImGui::Text("Frame time: %.2f ms (%.1f FPS)", deltaTime * 1000.0f, io.Framerate);
            ImGui::Text("Dispatches: %d | VRAM: %.1f MB", halfResShadowsEnabled ? 3 : 2, vramMB);
            ImGui::Text("Render Resolution: %d x %d", targets.width, targets.height);
            ImGui::Text("Pending Chunk Gen (Hot): %zu", requestQueue.size());
            ImGui::Text("GPU Chunks: %zu / %d slots (%d pool%s)%s", chunkSlotMap.size(), totalSlots, poolCount,
                poolCount == 1 ? "" : "s", poolsExhausted ? " [VRAM FULL]" : "");
            ImGui::Text("CPU Chunks: %zu", worldMap.size());
            ImGui::Text("Animated Grass Tufts: %zu (within %d chunks)", activePlants.size(), R_GRASS_ANIM);
            ImGui::Text("Player Chunk: (%d, %d, %d)", playerChunk.x, playerChunk.y, playerChunk.z);

            ImGui::Separator();

            // Render Minimap
            ImGui::Text("--- RADAR SCANNER ---");
            ImGui::Image((ImTextureID)(intptr_t)minimapTex, ImVec2(128, 128));

            ImGui::Separator();

            // Display interactive editor tools
            ImGui::Text("--- CONTROLS & EDITOR ---");
            if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Press [TAB] to free mouse for UI.");
            } else {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Press [TAB] to capture cursor.");
            }

            ImGui::Checkbox("Paint Mode [P] (Right Click places)", &paintModeEnabled);
            ImGui::Checkbox("Chunk Viewer [C]", &chunkViewerEnabled);
            ImGui::Checkbox("Light Visualizer [L]", &lightVisualizerEnabled);

            int renderDistance = R_ACTIVE;
            if (ImGui::SliderInt("Render Distance [PgUp/PgDn]", &renderDistance, MIN_RENDER_DISTANCE, MAX_RENDER_DISTANCE, "%d chunks")) {
                setRenderDistance(renderDistance);
            }

            int renderScalePercent = (int)std::lround(renderScale * 100.0f);
            if (ImGui::SliderInt("Render Scale", &renderScalePercent, (int)(MIN_RENDER_SCALE * 100.0f), 100, "%d%%")) {
                renderScale = renderScalePercent / 100.0f; // Targets are reallocated next frame
            }
            ImGui::Checkbox("Half-Res Shadows", &halfResShadowsEnabled);

            ImGui::Separator();

            ImGui::Text("Selected Voxel Material:");
            const char* materials[] = { "Grass", "Dirt", "Stone", "Sand", "Plant" };
            for (int i = 0; i < 5; i++) {
                if (ImGui::RadioButton(materials[i], selectedBlockID == (i + 1))) {
                    selectedBlockID = i + 1;
                }
                if (i < 4) ImGui::SameLine();
            }

            ImGui::End();
        }

        if (minimized) {
            // Nothing to draw into; keep the UI frame balanced and wait for events
            ImGui::Render();
            glfwWaitEvents();
            continue;
        }

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)targets.width / (float)targets.height, 0.1f, 100.0f);

        // --- 0. OCEAN: evolve the wave spectrum and inverse-FFT it (Ocean.cpp, ocean.comp) ---
        ocean.Update((float)glfwGetTime());

        // --- 1. GPU ANIMATION PASS (grass around the player, see anim.comp) ---
        if (!activePlants.empty()) {
            glUseProgram(animProgram);
            glUniform1f(au.time, (float)glfwGetTime());
            glUniform1i(au.numPlants, (int)activePlants.size());
            glUniform2f(au.windDir, WIND_DIR.x, WIND_DIR.y);
            glm::ivec3 animPoolBase = poolBaseSlots();
            glUniform3iv(au.poolBase, 1, &animPoolBase[0]);

            // Pools as read-write images on units 0-3, their brick masks on units 4-7
            for (int p = 0; p < MAX_POOLS; p++) {
                glBindImageTexture(p, p < poolCount ? pools[p].voxels : 0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8);
                glBindImageTexture(4 + p, p < poolCount ? pools[p].bricks : 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R8);
            }
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, pageTableTex);
            glUniform1i(au.pageTable, 1);

            GLuint numGroups = ((GLuint)activePlants.size() + 63) / 64;

            // Erase the old pose of every tuft that moved...
            glUniform1i(au.mode, 0);
            glDispatchCompute(numGroups, 1, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

            // ...then draw every tuft, so overlapping neighbours are always restored
            glUniform1i(au.mode, 1);
            glDispatchCompute(numGroups, 1, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
        }

        // --- Per-frame values shared by the three render passes ---

        // Chunk pools and their brick masks (units fixed by layout bindings in default.comp).
        // Unallocated pools bind texture 0; the shader never samples them.
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, pageTableTex);
        glm::ivec3 renderPoolBase = poolBaseSlots();
        for (int p = 0; p < MAX_POOLS; p++) {
            glActiveTexture(POOL_TEXTURE_UNITS[p]);
            glBindTexture(GL_TEXTURE_3D, p < poolCount ? pools[p].voxels : 0);
            glActiveTexture(BRICK_TEXTURE_UNITS[p]);
            glBindTexture(GL_TEXTURE_3D, p < poolCount ? pools[p].bricks : 0);
        }

        // Coastline data for the waves (needs the page table and pools bound above)
        glm::ivec2 playerColumn = glm::ivec2(glm::floor(glm::vec2(cameraPos.x, cameraPos.z) * VOXELS_PER_UNIT));
        shore.Update(playerColumn, glfwGetTime(), renderPoolBase, SEA_LEVEL);
        glActiveTexture(GL_TEXTURE12);
        glBindTexture(GL_TEXTURE_2D, shore.Texture());

        // FFT ocean fields on units 10 and 11 (layout bindings in default.comp)
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D_ARRAY, ocean.DisplacementTexture());
        glActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_2D_ARRAY, ocean.SlopeTexture());

        // Render targets as images: 0 = final color, 2 = G-buffer, 3/4 = half-res shadow value/key
        glBindImageTexture(0, targets.color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindImageTexture(2, targets.gBuffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32I);
        glBindImageTexture(3, targets.shadowValue, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        glBindImageTexture(4, targets.shadowKey, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32I);

        // Project Strata: Scan for crosshair color (drawn by ImGui below)
        glm::vec3 crosshairColor(1.0f, 1.0f, 1.0f);
        RayHit chHit = raycast(cameraPos, cameraFront, REACH_DISTANCE);
        if (chHit.hit) {
            GLubyte val = getVoxelFromChunks(chHit.mapPos.x, chHit.mapPos.y, chHit.mapPos.z);
            if (val == 32) {
                crosshairColor = glm::vec3(1.0f, 0.0f, 1.0f); // Magenta for artifact
            } else if (val > 0) {
                crosshairColor = glm::vec3(0.1f, 1.0f, 0.3f); // Green for terrain
            }
        }

        float timeNow = (float)glfwGetTime();

        // --- Day-Night Cycle Calculations ---
        const float DAY_DURATION = 600.0f; // 600 seconds (10 minutes) per full cycle
        const float PI = 3.14159265f;
        float angle = (timeNow / DAY_DURATION) * 2.0f * PI;

        // Calculate Sun and Moon directions
        glm::vec3 sunDir = glm::normalize(glm::vec3(cos(angle), sin(angle), 0.4f));
        glm::vec3 moonDir = -sunDir;

        // Determine active light source and interpolate parameters
        float t = sunDir.y;
        glm::vec3 skyColor;
        glm::vec3 lightColor;
        glm::vec3 lightDir;
        float ambient;

        if (t > 0.0f) {
            // Daytime: Sun is active
            skyColor = glm::mix(glm::vec3(0.8f, 0.4f, 0.3f), glm::vec3(0.4f, 0.7f, 1.0f), t);
            lightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.3f), glm::vec3(1.0f, 0.95f, 0.85f), t);
            lightDir = sunDir;
            ambient = glm::mix(0.15f, 0.25f, t);
        } else {
            // Nighttime: Moon is active
            float nightFactor = glm::clamp(-t * 4.0f, 0.0f, 1.0f);
            skyColor = glm::mix(glm::vec3(0.8f, 0.4f, 0.3f), glm::vec3(0.01f, 0.02f, 0.05f), nightFactor);
            lightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.3f), glm::vec3(0.08f, 0.12f, 0.20f), nightFactor);
            lightDir = moonDir;
            ambient = glm::mix(0.15f, 0.05f, nightFactor);
        }

        glm::mat4 inverseView = glm::inverse(view);
        glm::mat4 inverseProj = glm::inverse(proj);
        int numFlaresVal = (int)std::min<size_t>(activeFlares.size(), 8);

        // Uniforms are per program, so each pass gets the same set (unused ones are -1 and ignored)
        auto uploadFrameUniforms = [&](const ComputeUniforms& u) {
            glUniform1i(u.pageTable, 1); // Unit 1
            glUniform3iv(u.poolBase, 1, &renderPoolBase[0]); // First slot of pools 1..3
            glUniform2i(u.renderSize, targets.width, targets.height);
            glUniform1i(u.halfResShadows, halfResShadowsEnabled ? 1 : 0);
            glUniform1i(u.chunkViewerEnabled, chunkViewerEnabled ? 1 : 0);
            glUniform1i(u.lightVisualizerEnabled, lightVisualizerEnabled ? 1 : 0);

            // Project Strata: laser beam
            glUniform1i(u.laserBeamActive, laserBeamActive ? 1 : 0);
            glUniform3fv(u.laserBeamStart, 1, &laserBeamStart[0]);
            glUniform3fv(u.laserBeamEnd, 1, &laserBeamEnd[0]);
            glUniform3fv(u.laserBeamColor, 1, &laserBeamColor[0]);

            // Project Strata: active flares
            glUniform1i(u.numFlares, numFlaresVal);
            for (int i = 0; i < numFlaresVal; i++) {
                glUniform3fv(u.flarePos[i], 1, &activeFlares[i].pos[0]);
                glUniform3fv(u.flareColor[i], 1, &activeFlares[i].color[0]);
                glUniform1f(u.flareIntensity[i], activeFlares[i].intensity);
            }

            // Project Strata: held clump (gravity tether)
            glUniform1i(u.heldClumpActive, heldClumpActive ? 1 : 0);
            glUniform3fv(u.heldClumpPos, 1, &heldClumpPos[0]);
            glUniform1f(u.heldClumpRadius, heldClumpRadius);
            glUniform1i(u.heldClumpIsArtifact, heldClumpIsArtifact ? 1 : 0);

            // Rays stop one chunk past the upload radius (18 chunks -> 608 voxels, the old fixed value)
            glUniform1i(u.renderDistanceVoxels, (R_ACTIVE + 1) * CHUNK_SIZE);
            glUniform1i(u.seaLevel, SEA_LEVEL);
            glm::vec3 tileSizes = ocean.TileSizes();
            glUniform3fv(u.oceanTileSizes, 1, &tileSizes[0]);
            glUniform1f(u.oceanChoppiness, OCEAN_CHOPPINESS);
            glm::ivec2 shoreOriginColumn = shore.Origin();
            glUniform2i(u.shoreOrigin, shoreOriginColumn.x, shoreOriginColumn.y);
            glm::vec3 beacon = glm::vec3(spawnVoxel.x, 0, spawnVoxel.z) / VOXELS_PER_UNIT;
            glUniform3fv(u.beaconPos, 1, &beacon[0]);

            // dimX is the world->voxel scale in the shader; dimY the vertical extent
            glUniform1i(u.dimX, (int)VOXELS_PER_UNIT);
            glUniform1i(u.dimY, WORLD_HEIGHT);
            glUniform1i(u.dimZ, (int)VOXELS_PER_UNIT);
            glUniform3fv(u.cameraPos, 1, &cameraPos[0]);
            glUniformMatrix4fv(u.inverseView, 1, GL_FALSE, glm::value_ptr(inverseView));
            glUniformMatrix4fv(u.inverseProj, 1, GL_FALSE, glm::value_ptr(inverseProj));
            glUniform1f(u.time, timeNow);

            // Day-Night cycle
            glUniform3fv(u.sunDir, 1, &sunDir[0]);
            glUniform3fv(u.moonDir, 1, &moonDir[0]);
            glUniform3fv(u.lightDir, 1, &lightDir[0]);
            glUniform3fv(u.lightColor, 1, &lightColor[0]);
            glUniform3fv(u.skyColor, 1, &skyColor[0]);
            glUniform1f(u.ambient, ambient);
        };

        GLuint fullGroupsX = (targets.width + 7) / 8, fullGroupsY = (targets.height + 7) / 8;

        // --- 2. TRACE PASS: one camera ray per pixel into the G-buffer ---
        glUseProgram(tracePass.program);
        uploadFrameUniforms(tracePass.uniforms);
        glDispatchCompute(fullGroupsX, fullGroupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // --- 3. SHADOW PASS: soft shadows once per 2x2 quad ---
        if (halfResShadowsEnabled) {
            glUseProgram(shadowPass.program);
            uploadFrameUniforms(shadowPass.uniforms);
            int quadsX = (targets.width + 1) / 2, quadsY = (targets.height + 1) / 2;
            glDispatchCompute((quadsX + 7) / 8, (quadsY + 7) / 8, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

        // --- 4. SHADE PASS: lighting and overlays into the final color image ---
        glUseProgram(shadePass.program);
        uploadFrameUniforms(shadePass.uniforms);
        glDispatchCompute(fullGroupsX, fullGroupsY, 1);

        // Make the image writes visible to the blit below
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

        // Render Pass: Blit, stretching the (possibly smaller) render target to the window
        bool scaled = targets.width != fbWidth || targets.height != fbHeight;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Screen
        glBlitFramebuffer(0, 0, targets.width, targets.height, 0, 0, fbWidth, fbHeight, GL_COLOR_BUFFER_BIT, scaled ? GL_LINEAR : GL_NEAREST);

        // Crosshair and hotbar at full window resolution, regardless of render scale
        drawHud(crosshairColor);

        // Render ImGui UI on top of the screen
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup Dear ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Stop workers: one dummy request per thread wakes each from wait_and_pop
    workerRunning = false;
    for (size_t i = 0; i < workerThreads.size(); i++) {
        requestQueue.push({ 0, 0, 0 });
    }
    for (auto& t : workerThreads) {
        if (t.joinable()) t.join();
    }

    for (auto& pair : worldMap) delete pair.second;
    worldMap.clear();

    glfwTerminate();
    return 0;
}
