#include "World/VoxelWorld.h"

Chunk* VoxelWorld::FindChunk(int cx, int cy, int cz) const {
    auto it = m_Chunks.find(ChunkKey(cx, cy, cz));
    return it != m_Chunks.end() ? it->second.get() : nullptr;
}

Chunk* VoxelWorld::GetOrCreateChunk(int cx, int cy, int cz) {
    std::unique_ptr<Chunk>& slot = m_Chunks[ChunkKey(cx, cy, cz)];
    if (!slot) {
        slot = std::make_unique<Chunk>();
        slot->position = glm::ivec3(cx, cy, cz);
    }
    return slot.get();
}

uint8_t VoxelWorld::GetVoxel(int x, int y, int z) const {
    if (y < 0 || y >= WORLD_HEIGHT) return 0;
    Chunk* chunk = FindChunk(x >> 5, y >> 5, z >> 5);
    if (!chunk || chunk->data.empty()) return 0;
    return chunk->data[LocalIndex(x & 31, y & 31, z & 31)];
}

bool VoxelWorld::SetVoxel(int x, int y, int z, uint8_t id) {
    if (y < 0 || y >= WORLD_HEIGHT) return false;
    Chunk* chunk = FindChunk(x >> 5, y >> 5, z >> 5);
    if (!chunk) return false;
    if (chunk->data.empty()) {
        if (id == 0) return true; // Already air
        chunk->data.assign(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, 0);
    }
    chunk->data[LocalIndex(x & 31, y & 31, z & 31)] = id;
    chunk->isModified = true;
    return true;
}

void VoxelWorld::FreeChunkIfUnmodified(int cx, int cy, int cz) {
    auto it = m_Chunks.find(ChunkKey(cx, cy, cz));
    if (it != m_Chunks.end() && !it->second->isModified) m_Chunks.erase(it);
}
