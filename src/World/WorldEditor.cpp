#include "World/WorldEditor.h"

#include "World/Chunk.h"
#include "World/ChunkStreamer.h"
#include "World/VoxelWorld.h"

#include <algorithm>
#include <vector>

WorldEditor::WorldEditor(VoxelWorld& world, ChunkStreamer& streamer) : m_World(world), m_Streamer(streamer) {}

// Each touched chunk is re-uploaded once per edit, not once per voxel
void WorldEditor::RefreshTouched() {
    for (uint64_t key : m_Touched) m_Streamer.RefreshChunk(key);
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
                if (m_World.SetVoxel(px, py, pz, id)) {
                    uint64_t key = ChunkKey(px >> 5, py >> 5, pz >> 5);
                    if (std::find(m_Touched.begin(), m_Touched.end(), key) == m_Touched.end()) m_Touched.push_back(key);
                }
            }
        }
    }
    RefreshTouched();
}

void WorldEditor::WriteBox(glm::ivec3 minCorner, glm::ivec3 size, const std::vector<uint8_t>& ids) {
    if (ids.size() < (size_t)size.x * size.y * size.z) return;
    WriteBox(minCorner, size, ids.data(), 0);
}

void WorldEditor::FillBox(glm::ivec3 minCorner, glm::ivec3 size, uint8_t id) {
    WriteBox(minCorner, size, nullptr, id);
}

void WorldEditor::WriteBox(glm::ivec3 minCorner, glm::ivec3 size, const uint8_t* ids, uint8_t fill) {
    size_t i = 0;
    for (int y = 0; y < size.y; y++) {
        for (int z = 0; z < size.z; z++) {
            for (int x = 0; x < size.x; x++, i++) {
                glm::ivec3 p = minCorner + glm::ivec3(x, y, z);
                if (m_World.SetVoxel(p.x, p.y, p.z, ids ? ids[i] : fill)) {
                    uint64_t key = ChunkKey(p.x >> 5, p.y >> 5, p.z >> 5);
                    if (std::find(m_Touched.begin(), m_Touched.end(), key) == m_Touched.end()) m_Touched.push_back(key);
                }
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

bool WorldEditor::ConsumeTerrainChanged() {
    bool changed = m_TerrainChanged;
    m_TerrainChanged = false;
    return changed;
}
