#pragma once

#include "Core/SafeQueue.h"
#include "Rendering/GpuChunkCache.h"

#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <unordered_set>
#include <vector>

class VoxelWorld;
struct VoxModel;

// Decides which chunks exist where, around the player, and keeps them there:
//  - Background worker threads generate chunks (TerrainGenerator).
//  - The GPU gets every non-empty chunk within the render distance (GpuChunkCache).
//  - The CPU keeps voxel data only within CPU_RADIUS (collision, raycasts, editing) plus every
//    chunk the player edited, so RAM does not grow with the render distance.
// Each of these is a square window of chunk columns; when the player moves, only the columns
// entering or leaving a window are processed.
class ChunkStreamer {
public:
    static constexpr int MIN_RENDER_DISTANCE = 4;
    static constexpr int MAX_RENDER_DISTANCE = 128;
    static constexpr int GPU_EVICT_MARGIN = 2;                // GPU chunks are evicted this far past the render distance
    static constexpr int CPU_RADIUS = 16;                     // CPU voxel data is loaded within this radius...
    static constexpr int CPU_KEEP_RADIUS = CPU_RADIUS + 2;    // ...and freed (unless edited) beyond this one

    ChunkStreamer(VoxelWorld& world, GpuChunkCache& cache, const VoxModel& trees);
    ~ChunkStreamer();

    // Starts the worker threads. The tree model must be loaded already.
    void Start(int renderDistance);
    void Stop();

    // Records the player's chunk; returns true if it moved to a different column. Call
    // UpdateWindows afterwards when it did (after integrating this frame's results).
    bool SetPlayerChunk(glm::ivec3 chunk);
    glm::ivec3 PlayerChunk() const { return m_PlayerChunk; }

    // Loads and frees chunks for the current player position and render distance
    void UpdateWindows();

    // Processes finished chunks until budgetSeconds have passed since frameStartTime
    void IntegrateResults(double frameStartTime, double budgetSeconds);

    // Blocks until every layer of this chunk column has CPU data (used at spawn)
    void WaitForColumn(int cx, int cz);

    // Re-uploads an edited chunk, or makes it resident if it is close enough
    void RefreshChunk(uint64_t key);

    void SetRenderDistance(int chunks); // Clamped; re-streams immediately
    int RenderDistance() const { return m_RenderDistance; }
    size_t PendingRequests() { return m_Requests.Size(); }

private:
    struct Request {
        int cx, cy, cz;
    };

    struct Result {
        int cx, cy, cz;
        std::vector<uint8_t> data; // Empty when the chunk is all air
        std::vector<uint16_t> artifactIdx;
        uint8_t brickMask[BRICK_MASK_SIZE] = {};
        bool cancelled = false; // Player moved away before the worker got to it
    };

    // A square of chunk columns around the player; radius -1 means empty
    struct Window {
        glm::ivec2 center = glm::ivec2(0);
        int radius = -1;
        bool Contains(int cx, int cz) const;
    };

    void WorkerLoop();
    void RequestChunk(int cx, int cy, int cz);
    void Integrate(Result& result);
    int DistanceXZ(int cx, int cz) const;
    bool IsWanted(int cx, int cz) const;
    static std::vector<glm::ivec2> ColumnsEntering(const Window& from, const Window& to);

    VoxelWorld& m_World;
    GpuChunkCache& m_Cache;
    const VoxModel& m_Trees;

    glm::ivec3 m_PlayerChunk = glm::ivec3(0);
    int m_RenderDistance = 18; // GPU upload radius
    Window m_GpuLoadWindow, m_GpuKeepWindow, m_CpuLoadWindow, m_CpuKeepWindow;
    std::unordered_set<uint64_t> m_InFlight; // Requested from the workers, result not processed yet

    SafeQueue<Request> m_Requests;
    SafeQueue<Result> m_Results;
    std::vector<std::thread> m_Workers;
    std::atomic<bool> m_Running{ false };
    std::atomic<int> m_WorkerPlayerCx{ 0 }, m_WorkerPlayerCz{ 0 };
    std::atomic<int> m_CancelRadius{ 0 }; // Requests further than this from the player are skipped
};
