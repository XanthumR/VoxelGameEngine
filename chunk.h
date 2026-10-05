#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>
#include <unordered_map>

// --- World Layout ---
constexpr int CHUNK_SIZE = 32;
constexpr int CHUNK_LAYERS = 4;                         // Vertical chunk count
constexpr int WORLD_HEIGHT = CHUNK_SIZE * CHUNK_LAYERS; // 128 voxels
constexpr float VOXELS_PER_UNIT = 1280.0f;              // World space -> voxel space scale

// Block 34 is the grass tuft placed by the terrain generator and animated by anim.comp.
// Block 35 is water (the ocean up to SEA_LEVEL; its waves are applied while ray tracing).
// Both are drawn and cast shadows, but the player, raycasts and projectiles pass through them.
constexpr GLubyte GRASS_TUFT_ID = 34;
constexpr GLubyte WATER_ID = 35;

inline bool isSolidBlock(GLubyte id) {
    return id != 0 && id != GRASS_TUFT_ID && id != WATER_ID;
}

struct Chunk {
    glm::ivec3 position; // e.g., (0, 0, 0), (1, 0, 0)...
    std::vector<GLubyte> data; // CHUNK_SIZE^3 block IDs, or empty when the chunk is all air

    std::vector<uint16_t> artifactIdx; // Local indices that held an artifact (id 32) at generation time
    bool isModified = false;
};

// Your "World"
extern std::unordered_map<uint64_t, Chunk*> worldMap;

inline uint64_t getChunkKey(int cx, int cy, int cz) {
    // Pack 24 bits for cx, 8 bits for cy, 24 bits for cz
    return ((uint64_t)cx & 0xFFFFFF) | (((uint64_t)cy & 0xFF) << 24) | (((uint64_t)cz & 0xFFFFFF) << 32);
}

inline void unpackChunkKey(uint64_t key, int& cx, int& cy, int& cz) {
    int64_t x_raw = key & 0xFFFFFF;
    if (x_raw & 0x800000) x_raw |= ~(int64_t)0xFFFFFF;
    cx = (int)x_raw;

    cy = (int)((key >> 24) & 0xFF);

    int64_t z_raw = (key >> 32) & 0xFFFFFF;
    if (z_raw & 0x800000) z_raw |= ~(int64_t)0xFFFFFF;
    cz = (int)z_raw;
}

inline size_t localIndex(int lx, int ly, int lz) {
    return ((size_t)lz * CHUNK_SIZE * CHUNK_SIZE) + ((size_t)ly * CHUNK_SIZE) + lx;
}

// Lookup only: never allocates
inline Chunk* findChunk(int cx, int cy, int cz) {
    auto it = worldMap.find(getChunkKey(cx, cy, cz));
    return it != worldMap.end() ? it->second : nullptr;
}

inline Chunk* getOrCreateChunk(int cx, int cy, int cz) {
    uint64_t key = getChunkKey(cx, cy, cz);
    auto it = worldMap.find(key);
    if (it != worldMap.end()) return it->second;

    Chunk* chunk = new Chunk();
    chunk->position = glm::ivec3(cx, cy, cz);
    worldMap[key] = chunk;
    return chunk;
}

// Returns 0 (air) for unloaded or all-air chunks
inline GLubyte getVoxelFromChunks(int x, int y, int z) {
    if (y < 0 || y >= WORLD_HEIGHT) return 0;
    Chunk* chunk = findChunk(x >> 5, y >> 5, z >> 5);
    if (!chunk || chunk->data.empty()) return 0;
    return chunk->data[localIndex(x & 31, y & 31, z & 31)];
}

// Only writes into loaded chunks so a pending generation result can never clobber an edit
inline bool setVoxelInChunks(int x, int y, int z, GLubyte id) {
    if (y < 0 || y >= WORLD_HEIGHT) return false;
    Chunk* chunk = findChunk(x >> 5, y >> 5, z >> 5);
    if (!chunk) return false;
    if (chunk->data.empty()) {
        if (id == 0) return true; // Already air
        chunk->data.assign(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, 0);
    }
    chunk->data[localIndex(x & 31, y & 31, z & 31)] = id;
    chunk->isModified = true;
    return true;
}
