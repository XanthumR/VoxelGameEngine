#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class ChunkStreamer;
struct VoxModel;
class VoxelWorld;

// Player edits to the world: writes the CPU voxels, then re-uploads each touched chunk once
class WorldEditor {
public:
    WorldEditor(VoxelWorld& world, ChunkStreamer& streamer);

    // Sets every loaded voxel within radius of center (e.g. 0 to dig)
    void FillSphere(glm::ivec3 center, int radius, uint8_t id);
    bool PlaceVoxel(glm::ivec3 position, uint8_t id);

    // Writes a box of voxels (ids ordered x fastest, then z, then y), or fills it with one ID.
    // Voxels in unloaded chunks are skipped.
    void WriteBox(glm::ivec3 minCorner, glm::ivec3 size, const std::vector<uint8_t>& ids);
    void FillBox(glm::ivec3 minCorner, glm::ivec3 size, uint8_t id);

    // Removes a model's voxels at base (only voxels that still hold the model's block there), or
    // stamps it back (only into air). The model's bottom layer sits in the ground: erasing puts
    // groundId back there, and stamping may replace groundId. For felled and regrown trees.
    void StampModel(glm::ivec3 base, const VoxModel& model, bool erase, uint8_t groundId);

    // True once after any edit (the animated grass list needs rebuilding)
    bool ConsumeTerrainChanged();

private:
    void WriteBox(glm::ivec3 minCorner, glm::ivec3 size, const uint8_t* ids, uint8_t fill);
    void RefreshTouched();

    VoxelWorld& m_World;
    ChunkStreamer& m_Streamer;
    bool m_TerrainChanged = false;
    std::vector<uint64_t> m_Touched; // Chunk keys written by the current edit
};
