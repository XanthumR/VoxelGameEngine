#include "World/WorldEditor.h"

#include "World/Chunk.h"
#include "World/ChunkStreamer.h"
#include "World/VoxModel.h"
#include "World/VoxelWorld.h"

#include <algorithm>
#include <vector>

WorldEditor::WorldEditor(VoxelWorld& world, ChunkStreamer& streamer) : m_World(world), m_Streamer(streamer) {}

void WorldEditor::Touch(glm::ivec3 p) {
    uint64_t key = ChunkKey(p.x >> 5, p.y >> 5, p.z >> 5);
    glm::ivec3 local = p & 31;
    for (Touched& touched : m_Touched) {
        if (touched.key != key) continue;
        touched.min = glm::min(touched.min, local);
        touched.max = glm::max(touched.max, local);
        return;
    }
    m_Touched.push_back({ key, local, local });
}

// Each touched chunk is re-uploaded once per edit, not once per voxel, and only the box that changed
void WorldEditor::RefreshTouched() {
    for (const Touched& touched : m_Touched) m_Streamer.RefreshChunkBox(touched.key, touched.min, touched.max);
    if (!m_Touched.empty()) m_TerrainChanged = true;
    m_Touched.clear();
}

void WorldEditor::FillSphere(glm::ivec3 center, int radius, uint8_t id) {
    for (int dz = -radius; dz <= radius; dz++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy + dz * dz > radius * radius) continue;
                int px = center.x + dx;
                int py = center.y + dy;
                int pz = center.z + dz;
                if (m_World.SetVoxel(px, py, pz, id)) Touch(glm::ivec3(px, py, pz));
            }
        }
    }
    RefreshTouched();
}

void WorldEditor::WriteBox(glm::ivec3 minCorner, glm::ivec3 size, const std::vector<uint8_t>& ids, int solidOnlyLayers) {
    if (ids.size() < (size_t)size.x * size.y * size.z) return;
    WriteBox(minCorner, size, ids.data(), 0, solidOnlyLayers);
}

void WorldEditor::FillBox(glm::ivec3 minCorner, glm::ivec3 size, uint8_t id) {
    WriteBox(minCorner, size, nullptr, id, 0);
}

void WorldEditor::WriteBox(glm::ivec3 minCorner, glm::ivec3 size, const uint8_t* ids, uint8_t fill, int solidOnlyLayers) {
    size_t i = 0;
    for (int y = 0; y < size.y; y++) {
        for (int z = 0; z < size.z; z++) {
            for (int x = 0; x < size.x; x++, i++) {
                glm::ivec3 p = minCorner + glm::ivec3(x, y, z);
                uint8_t id = ids ? ids[i] : fill;
                if (id == 0 && y < solidOnlyLayers) continue; // Keep the ground or sea around it
                if (m_World.GetVoxel(p.x, p.y, p.z) == id) continue; // Unchanged: nothing to upload
                if (m_World.SetVoxel(p.x, p.y, p.z, id)) Touch(p);
            }
        }
    }
    RefreshTouched();
}

bool WorldEditor::PlaceVoxel(glm::ivec3 position, uint8_t id) {
    if (!m_World.SetVoxel(position.x, position.y, position.z, id)) return false;
    m_TerrainChanged = true;
    m_Streamer.RefreshChunk(ChunkKey(position.x >> 5, position.y >> 5, position.z >> 5));
    return true;
}

void WorldEditor::StampModel(glm::ivec3 base, const VoxModel& model, bool erase, uint8_t groundId) {
    for (const VoxelOffset& offset : model.voxels) {
        glm::ivec3 p = base + glm::ivec3(offset.x, offset.y, offset.z);
        uint8_t current = m_World.GetVoxel(p.x, p.y, p.z);
        bool ground = offset.y == 0;
        uint8_t empty = ground ? groundId : (uint8_t)0; // What the voxel is without the model
        if (erase ? current != offset.blockType : current != empty) continue;
        if (m_World.SetVoxel(p.x, p.y, p.z, erase ? empty : (uint8_t)offset.blockType)) Touch(p);
    }
    RefreshTouched();
}

bool WorldEditor::ConsumeTerrainChanged() {
    bool changed = m_TerrainChanged;
    m_TerrainChanged = false;
    return changed;
}
