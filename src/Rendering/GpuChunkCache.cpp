#include "Rendering/GpuChunkCache.h"

#include "World/Chunk.h"

#include <algorithm>
#include <climits>
#include <iostream>

namespace {

const int POOL_PLANNED_LAYERS[GpuChunkCache::MAX_POOLS] = { 2, 6, 16, 32 }; // 256 MB, 768 MB, 2 GB, 4 GB
const GLenum POOL_TEXTURE_UNITS[GpuChunkCache::MAX_POOLS] = { GL_TEXTURE2, GL_TEXTURE4, GL_TEXTURE6, GL_TEXTURE8 };
const GLenum BRICK_TEXTURE_UNITS[GpuChunkCache::MAX_POOLS] = { GL_TEXTURE3, GL_TEXTURE5, GL_TEXTURE7, GL_TEXTURE9 };

// Allocates an empty R8 3D texture with nearest filtering
void Allocate3DTexture(GLuint texture, int width, int height, int depth) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_R8, width, height, depth);
}

int WrapPageTable(int coordinate) {
    int wrapped = coordinate % GpuChunkCache::PAGE_TABLE_WRAP;
    return wrapped < 0 ? wrapped + GpuChunkCache::PAGE_TABLE_WRAP : wrapped;
}

} // namespace

bool GpuChunkCache::Init() {
    glGenTextures(1, &m_PageTable);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, m_PageTable);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32I, PAGE_TABLE_WRAP, CHUNK_LAYERS, PAGE_TABLE_WRAP, 0, GL_RGBA_INTEGER, GL_INT, m_PageTableData.data());

    // Further pools are added on demand as the render distance grows
    if (!AddPool()) {
        std::cerr << "Failed to allocate the chunk pool." << std::endl;
        return false;
    }
    return true;
}

bool GpuChunkCache::ComputeBrickMask(const std::vector<uint8_t>& data, uint8_t* brickMask) {
    std::fill(brickMask, brickMask + BRICK_MASK_SIZE, 0);
    if (data.empty()) return false;
    bool hasBlocks = false;
    for (int z = 0; z < CHUNK_SIZE; z++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                if (data[LocalIndex(x, y, z)] > 0) {
                    brickMask[((z / BRICK_SIZE) * BRICKS_PER_AXIS + (y / BRICK_SIZE)) * BRICKS_PER_AXIS + (x / BRICK_SIZE)] = 255;
                    hasBlocks = true;
                }
            }
        }
    }
    return hasBlocks;
}

// Adds the next pool texture (and its brick mask) and puts its slots on the free list.
// Halves the planned size if the driver reports out of memory.
bool GpuChunkCache::AddPool() {
    if (m_PoolCount == MAX_POOLS) {
        m_Exhausted = true;
        return false;
    }
    int baseSlot = m_PoolCount == 0 ? 0 : m_Pools[m_PoolCount - 1].baseSlot + m_Pools[m_PoolCount - 1].layers * SLOTS_PER_LAYER;

    for (int layers = POOL_PLANNED_LAYERS[m_PoolCount]; layers >= 1; layers /= 2) {
        GLuint textures[2];
        glGenTextures(2, textures);
        while (glGetError() != GL_NO_ERROR) {} // Clear stale errors so we only see ours
        Allocate3DTexture(textures[0], POOL_SLOTS_XY * CHUNK_SIZE, POOL_SLOTS_XY * CHUNK_SIZE, layers * CHUNK_SIZE);
        Allocate3DTexture(textures[1], POOL_SLOTS_XY * BRICKS_PER_AXIS, POOL_SLOTS_XY * BRICKS_PER_AXIS, layers * BRICKS_PER_AXIS);
        if (glGetError() == GL_OUT_OF_MEMORY) {
            glDeleteTextures(2, textures);
            continue;
        }

        m_Pools[m_PoolCount] = { textures[0], textures[1], baseSlot, layers };
        m_PoolCount++;
        // Push in reverse so the lowest new slot is handed out first
        for (int s = baseSlot + layers * SLOTS_PER_LAYER - 1; s >= baseSlot; s--) {
            m_FreeSlots.push_back(s);
        }
        std::cout << "Allocated chunk pool " << m_PoolCount << ": " << layers * SLOTS_PER_LAYER
                  << " slots (" << layers * 128 << " MB)" << std::endl;
        return true;
    }

    std::cerr << "Out of video memory for chunk pools; distant chunks will not be shown." << std::endl;
    m_Exhausted = true;
    return false;
}

glm::ivec3 GpuChunkCache::PoolBaseSlots() const {
    glm::ivec3 base(INT_MAX);
    for (int p = 1; p < m_PoolCount; p++) base[p - 1] = m_Pools[p].baseSlot;
    return base;
}

// Finds the pool holding a slot and the slot's chunk origin (in voxels) inside it
int GpuChunkCache::LocateSlot(int slotIndex, glm::ivec3& originVoxels) const {
    int p = m_PoolCount - 1;
    while (p > 0 && slotIndex < m_Pools[p].baseSlot) p--;
    int local = slotIndex - m_Pools[p].baseSlot;
    originVoxels = glm::ivec3(local % POOL_SLOTS_XY, (local / POOL_SLOTS_XY) % POOL_SLOTS_XY, local / SLOTS_PER_LAYER) * CHUNK_SIZE;
    return p;
}

void GpuChunkCache::UploadToSlot(int slotIndex, const std::vector<uint8_t>& data, const uint8_t* brickMask) {
    glm::ivec3 origin;
    int p = LocateSlot(slotIndex, origin);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, m_Pools[p].voxels);
    glTexSubImage3D(GL_TEXTURE_3D, 0, origin.x, origin.y, origin.z, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, GL_RED, GL_UNSIGNED_BYTE, data.data());

    UploadBrickMask(p, origin, brickMask);
}

void GpuChunkCache::UploadBrickMask(int pool, glm::ivec3 origin, const uint8_t* brickMask) {
    glm::ivec3 brickOrigin = origin / BRICK_SIZE;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, m_Pools[pool].bricks);
    glTexSubImage3D(GL_TEXTURE_3D, 0, brickOrigin.x, brickOrigin.y, brickOrigin.z,
        BRICKS_PER_AXIS, BRICKS_PER_AXIS, BRICKS_PER_AXIS, GL_RED, GL_UNSIGNED_BYTE, brickMask);
}

bool GpuChunkCache::UpdateRegion(uint64_t key, const std::vector<uint8_t>& data, glm::ivec3 min, glm::ivec3 max) {
    auto it = m_ChunkSlots.find(key);
    if (it == m_ChunkSlots.end()) return false;
    uint8_t brickMask[BRICK_MASK_SIZE];
    if (!ComputeBrickMask(data, brickMask)) return false;

    glm::ivec3 origin;
    int p = LocateSlot(it->second, origin);
    glm::ivec3 size = max - min + 1;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, m_Pools[p].voxels);
    // Rows and slices of the box are read from the whole chunk's data
    glPixelStorei(GL_UNPACK_ROW_LENGTH, CHUNK_SIZE);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, CHUNK_SIZE);
    glTexSubImage3D(GL_TEXTURE_3D, 0, origin.x + min.x, origin.y + min.y, origin.z + min.z, size.x, size.y, size.z, GL_RED,
        GL_UNSIGNED_BYTE, data.data() + LocalIndex(min.x, min.y, min.z));
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    UploadBrickMask(p, origin, brickMask);
    return true;
}

void GpuChunkCache::WritePageTable(int cx, int cy, int cz, glm::ivec4 value) {
    int ptX = WrapPageTable(cx), ptZ = WrapPageTable(cz);
    m_PageTableData[(ptZ * CHUNK_LAYERS + cy) * PAGE_TABLE_WRAP + ptX] = value;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, m_PageTable);
    glTexSubImage3D(GL_TEXTURE_3D, 0, ptX, cy, ptZ, 1, 1, 1, GL_RGBA_INTEGER, GL_INT, &value);
}

const glm::ivec4& GpuChunkCache::ReadPageTable(int cx, int cy, int cz) const {
    return m_PageTableData[(WrapPageTable(cz) * CHUNK_LAYERS + cy) * PAGE_TABLE_WRAP + WrapPageTable(cx)];
}

void GpuChunkCache::MakeResident(uint64_t key, glm::ivec3 chunkCoord, const std::vector<uint8_t>& data, const uint8_t* brickMask) {
    uint8_t computedMask[BRICK_MASK_SIZE];
    bool hasBlocks;
    if (brickMask) {
        hasBlocks = !data.empty();
    } else {
        hasBlocks = ComputeBrickMask(data, computedMask);
        brickMask = computedMask;
    }

    auto it = m_ChunkSlots.find(key);
    if (!hasBlocks) {
        if (it != m_ChunkSlots.end()) Evict(key); // Dug out completely
        m_KnownEmpty.insert(key);
        return;
    }
    m_KnownEmpty.erase(key);

    if (it != m_ChunkSlots.end()) {
        UploadToSlot(it->second, data, brickMask);
        return;
    }

    if (m_FreeSlots.empty() && (m_Exhausted || !AddPool())) return;

    int slotIndex = m_FreeSlots.back();
    m_FreeSlots.pop_back();
    m_ChunkSlots[key] = slotIndex;

    UploadToSlot(slotIndex, data, brickMask);
    WritePageTable(chunkCoord.x, chunkCoord.y, chunkCoord.z,
        glm::ivec4(slotIndex + 1, chunkCoord.x, chunkCoord.y, chunkCoord.z));
}

void GpuChunkCache::Evict(uint64_t key) {
    auto it = m_ChunkSlots.find(key);
    if (it == m_ChunkSlots.end()) return;
    m_FreeSlots.push_back(it->second);
    m_ChunkSlots.erase(it);

    int cx, cy, cz;
    UnpackChunkKey(key, cx, cy, cz);

    // Only clear the page table if it still points at this chunk (not a wrapped neighbour)
    const glm::ivec4& entry = ReadPageTable(cx, cy, cz);
    if (entry.y == cx && entry.z == cy && entry.w == cz) {
        WritePageTable(cx, cy, cz, glm::ivec4(0));
    }
}

void GpuChunkCache::Forget(uint64_t key) {
    Evict(key);
    m_KnownEmpty.erase(key);
}

void GpuChunkCache::BindForSampling() const {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, m_PageTable);
    for (int p = 0; p < MAX_POOLS; p++) {
        // Unallocated pools bind texture 0; the shaders never sample them
        glActiveTexture(POOL_TEXTURE_UNITS[p]);
        glBindTexture(GL_TEXTURE_3D, p < m_PoolCount ? m_Pools[p].voxels : 0);
        glActiveTexture(BRICK_TEXTURE_UNITS[p]);
        glBindTexture(GL_TEXTURE_3D, p < m_PoolCount ? m_Pools[p].bricks : 0);
    }
}

void GpuChunkCache::BindAsImages() const {
    for (int p = 0; p < MAX_POOLS; p++) {
        glBindImageTexture(p, p < m_PoolCount ? m_Pools[p].voxels : 0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8);
        glBindImageTexture(4 + p, p < m_PoolCount ? m_Pools[p].bricks : 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R8);
    }
}

int GpuChunkCache::TotalSlots() const {
    int total = 0;
    for (int p = 0; p < m_PoolCount; p++) total += m_Pools[p].layers * SLOTS_PER_LAYER;
    return total;
}

double GpuChunkCache::PoolBytes() const {
    return TotalSlots() * (32768.0 + 64.0); // Voxels + brick mask per slot
}
