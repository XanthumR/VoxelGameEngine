#pragma once

#include "World/WorldConstants.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

// CPU copy of one CHUNK_SIZE^3 block of voxels
struct Chunk {
    glm::ivec3 position;               // Chunk coordinates (voxel position / CHUNK_SIZE)
    std::vector<uint8_t> data;         // CHUNK_SIZE^3 block IDs, or empty when the chunk is all air
    bool isModified = false;           // Edited by the player: kept forever, never regenerated
};

// Packs 24 bits of cx, 8 bits of cy and 24 bits of cz into one key
inline uint64_t ChunkKey(int cx, int cy, int cz) {
    return ((uint64_t)cx & 0xFFFFFF) | (((uint64_t)cy & 0xFF) << 24) | (((uint64_t)cz & 0xFFFFFF) << 32);
}

inline void UnpackChunkKey(uint64_t key, int& cx, int& cy, int& cz) {
    int64_t xRaw = key & 0xFFFFFF;
    if (xRaw & 0x800000) xRaw |= ~(int64_t)0xFFFFFF;
    cx = (int)xRaw;

    cy = (int)((key >> 24) & 0xFF);

    int64_t zRaw = (key >> 32) & 0xFFFFFF;
    if (zRaw & 0x800000) zRaw |= ~(int64_t)0xFFFFFF;
    cz = (int)zRaw;
}

// Index of a voxel inside chunk data: x fastest, then y, then z (matches the GPU upload layout)
inline size_t LocalIndex(int lx, int ly, int lz) {
    return ((size_t)lz * CHUNK_SIZE * CHUNK_SIZE) + ((size_t)ly * CHUNK_SIZE) + lx;
}
