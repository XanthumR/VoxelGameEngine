#pragma once

#include <glm/glm.hpp>

#include <cstdint>

class ChunkStreamer;
class VoxelWorld;

// Player edits to the world: writes the CPU voxels, then re-uploads each touched chunk once
class WorldEditor {
public:
    WorldEditor(VoxelWorld& world, ChunkStreamer& streamer);

    // Sets every loaded voxel within radius of center (e.g. 0 to dig)
    void FillSphere(glm::ivec3 center, int radius, uint8_t id);
    bool PlaceVoxel(glm::ivec3 position, uint8_t id);

    // True once after any edit (the animated grass list needs rebuilding)
    bool ConsumeTerrainChanged();

private:
    VoxelWorld& m_World;
    ChunkStreamer& m_Streamer;
    bool m_TerrainChanged = false;
};
