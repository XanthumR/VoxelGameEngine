#pragma once

#include "World/Chunk.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

// The CPU copy of the voxel world: chunks near the player plus every chunk the player edited.
// Far chunks exist only on the GPU (see GpuChunkCache); ChunkStreamer decides what is loaded.
class VoxelWorld {
public:
    Chunk* FindChunk(int cx, int cy, int cz) const; // Lookup only, never allocates
    Chunk* GetOrCreateChunk(int cx, int cy, int cz);

    // Returns air for unloaded or all-air chunks
    uint8_t GetVoxel(int x, int y, int z) const;

    // Only writes into loaded chunks, so a pending generation result can never clobber an edit.
    // Marks the chunk as modified.
    bool SetVoxel(int x, int y, int z, uint8_t id);

    void FreeChunkIfUnmodified(int cx, int cy, int cz);
    size_t ChunkCount() const { return m_Chunks.size(); }

    template <typename Function>
    void ForEachChunk(Function function) {
        for (auto& entry : m_Chunks) function(*entry.second);
    }

private:
    std::unordered_map<uint64_t, std::unique_ptr<Chunk>> m_Chunks;
};
