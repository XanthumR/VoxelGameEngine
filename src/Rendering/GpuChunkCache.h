#pragma once

#include "World/WorldConstants.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Brick occupancy masks: one flag per 8^3 brick of a chunk, so rays can skip empty bricks
constexpr int BRICK_SIZE = 8;
constexpr int BRICKS_PER_AXIS = CHUNK_SIZE / BRICK_SIZE;                         // 4
constexpr int BRICK_MASK_SIZE = BRICKS_PER_AXIS * BRICKS_PER_AXIS * BRICKS_PER_AXIS; // 64

// The voxel data the GPU ray marcher reads, as a virtual texture:
//  - Pool textures: up to MAX_POOLS 3D textures, each a 64 x 64 x (32 * layers) grid of 32^3
//    chunk slots (one layer = 4096 slots = 128 MB). A new pool is allocated only when the
//    previous ones are full, so VRAM follows the render distance; existing slots never move.
//    Each pool has a brick mask texture beside it.
//  - Page table: a toroidal PAGE_TABLE_WRAP x 4 x PAGE_TABLE_WRAP texture of
//    [slotIndex + 1, cx, cy, cz] that maps chunk coordinates to slots.
//  - Column tops: a PAGE_TABLE_WRAP x PAGE_TABLE_WRAP R32UI texture, per 32x32 chunk column
//    1 + the highest voxel y that may be solid there (0: none), from the chunks' brick masks and
//    the figures drawn on the GPU. Rays skip the air above it in one step.
// All-air chunks get no slot; they are remembered as "known empty" instead.
class GpuChunkCache {
public:
    // Must be wider than the GPU window (2 * (max render distance + evict margin) + 1 = 261)
    static constexpr int PAGE_TABLE_WRAP = 512;
    static constexpr int MAX_POOLS = 4;
    static constexpr int POOL_SLOTS_XY = 64;
    static constexpr int SLOTS_PER_LAYER = POOL_SLOTS_XY * POOL_SLOTS_XY;

    // Creates the page table and the first pool. Returns false if no pool could be allocated.
    bool Init();

    // Fills a chunk's BRICK_MASK_SIZE occupancy flags (255 = has a solid voxel) and returns
    // whether any voxel is solid. Layout: x fastest, then y, then z (matches the upload).
    static bool ComputeBrickMask(const std::vector<uint8_t>& data, uint8_t* brickMask);

    // Uploads chunk data, allocating a slot if it has none yet. brickMask may be null, in which
    // case it is computed here (used after edits).
    void MakeResident(uint64_t key, glm::ivec3 chunkCoord, const std::vector<uint8_t>& data, const uint8_t* brickMask);
    // Uploads only the box min..max (chunk-local voxels, inclusive) of an edited resident chunk, and
    // its brick mask. False when it has no slot or is now all air: MakeResident handles those.
    bool UpdateRegion(uint64_t key, const std::vector<uint8_t>& data, glm::ivec3 min, glm::ivec3 max);
    void Evict(uint64_t key);

    // Raises the column tops for this frame's GPU-drawn figures (smoke puffs), given as the
    // highest voxel each reaches; last frame's are lowered again. Within MAX_FIGURE_POINTS.
    static constexpr int MAX_FIGURE_POINTS = 8192;
    void SetFigureTops(const std::vector<glm::ivec3>& highestVoxels);
    void Forget(uint64_t key); // Evict and drop the "known empty" mark (chunk left the GPU window)

    bool IsResident(uint64_t key) const { return m_ChunkSlots.count(key) != 0; }
    bool IsKnownEmpty(uint64_t key) const { return m_KnownEmpty.count(key) != 0; }

    // First global slot of pools 1..3 for the shaders (INT_MAX = pool not allocated)
    glm::ivec3 PoolBaseSlots() const;

    // Column tops on unit 0, page table on unit 1, pools and brick masks on units 2-9 (layout
    // bindings in the shaders)
    void BindForSampling() const;
    // Pools as read-write images on units 0-3, brick masks as images on units 4-7
    void BindAsImages() const;

    GLuint PageTableTexture() const { return m_PageTable; }
    size_t ResidentCount() const { return m_ChunkSlots.size(); }
    int PoolCount() const { return m_PoolCount; }
    int TotalSlots() const;
    bool IsExhausted() const { return m_Exhausted; }
    double PoolBytes() const;
    double PageTableBytes() const { return (double)m_PageTableData.size() * sizeof(glm::ivec4); }

private:
    struct Pool {
        GLuint voxels = 0;
        GLuint bricks = 0;
        int baseSlot = 0; // First global slot index stored in this pool
        int layers = 0;
    };

    bool AddPool();
    int LocateSlot(int slotIndex, glm::ivec3& originVoxels) const;
    void UploadToSlot(int slotIndex, const std::vector<uint8_t>& data, const uint8_t* brickMask);
    void UploadBrickMask(int pool, glm::ivec3 origin, const uint8_t* brickMask);
    void WritePageTable(int cx, int cy, int cz, glm::ivec4 value);
    void SetChunkTop(int cx, int cy, int cz, int top); // Highest local y that may be solid, -1 none
    void UpdateColumn(int ptX, int ptZ);                // Re-uploads its top if it changed
    const glm::ivec4& ReadPageTable(int cx, int cy, int cz) const;

    GLuint m_PageTable = 0;
    std::vector<glm::ivec4> m_PageTableData = std::vector<glm::ivec4>(PAGE_TABLE_WRAP * CHUNK_LAYERS * PAGE_TABLE_WRAP, glm::ivec4(0));
    Pool m_Pools[MAX_POOLS];
    int m_PoolCount = 0;
    bool m_Exhausted = false; // Out of pools or VRAM; further chunks are not uploaded

    std::unordered_map<uint64_t, int> m_ChunkSlots; // Chunk key -> slot index
    std::vector<int> m_FreeSlots;
    std::unordered_set<uint64_t> m_KnownEmpty;      // All-air chunks in the GPU window (need no slot)

    // Column tops (see above): per page table entry its chunk's top, per column the figures' top
    // and the value on the GPU
    GLuint m_ColumnTops = 0;
    std::vector<int8_t> m_ChunkTops = std::vector<int8_t>(PAGE_TABLE_WRAP * CHUNK_LAYERS * PAGE_TABLE_WRAP, -1);
    std::vector<uint8_t> m_FigureTops = std::vector<uint8_t>(PAGE_TABLE_WRAP * PAGE_TABLE_WRAP, 0);
    std::vector<uint32_t> m_ColumnValues = std::vector<uint32_t>(PAGE_TABLE_WRAP * PAGE_TABLE_WRAP, 0);
    std::vector<int> m_FigureColumns, m_PreviousFigureColumns; // Column indices with a figure top
};
