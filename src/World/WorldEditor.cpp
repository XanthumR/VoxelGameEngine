#include "World/WorldEditor.h"

#include "World/Chunk.h"
#include "World/ChunkStreamer.h"
#include "World/VoxelWorld.h"

#include <algorithm>
#include <vector>

WorldEditor::WorldEditor(VoxelWorld& world, ChunkStreamer& streamer) : m_World(world), m_Streamer(streamer) {}

void WorldEditor::FillSphere(glm::ivec3 center, int radius, uint8_t id) {
    std::vector<uint64_t> touched;
    for (int dz = -radius; dz <= radius; dz++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy + dz * dz > radius * radius) continue;
                int px = center.x + dx;
                int py = center.y + dy;
                int pz = center.z + dz;
                if (m_World.SetVoxel(px, py, pz, id)) {
                    uint64_t key = ChunkKey(px >> 5, py >> 5, pz >> 5);
                    if (std::find(touched.begin(), touched.end(), key) == touched.end()) touched.push_back(key);
                }
            }
        }
    }
    // One upload per touched chunk instead of one per voxel
    for (uint64_t key : touched) m_Streamer.RefreshChunk(key);
    if (!touched.empty()) m_TerrainChanged = true;
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
