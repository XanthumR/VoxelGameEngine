#include "World/ChunkStreamer.h"

#include "World/TerrainGenerator.h"
#include "World/VoxelWorld.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>

ChunkStreamer::ChunkStreamer(VoxelWorld& world, GpuChunkCache& cache, const VoxModel& trees)
    : m_World(world), m_Cache(cache), m_Trees(trees) {}

ChunkStreamer::~ChunkStreamer() {
    Stop();
}

void ChunkStreamer::Start(int renderDistance) {
    m_RenderDistance = std::clamp(renderDistance, MIN_RENDER_DISTANCE, MAX_RENDER_DISTANCE);
    m_CancelRadius = std::max(m_RenderDistance + GPU_EVICT_MARGIN, CPU_KEEP_RADIUS);

    unsigned int hardwareThreads = std::thread::hardware_concurrency(); // May report 0
    unsigned int workerCount = hardwareThreads > 1 ? hardwareThreads - 1 : 1;
    m_Running = true;
    for (unsigned int i = 0; i < workerCount; i++) {
        m_Workers.emplace_back(&ChunkStreamer::WorkerLoop, this);
    }
}

void ChunkStreamer::Stop() {
    if (!m_Running) return;
    // One dummy request per thread wakes each from WaitAndPop
    m_Running = false;
    for (size_t i = 0; i < m_Workers.size(); i++) m_Requests.Push({ 0, 0, 0 });
    for (std::thread& worker : m_Workers) {
        if (worker.joinable()) worker.join();
    }
    m_Workers.clear();
}

void ChunkStreamer::WorkerLoop() {
    TerrainGenerator generator(m_Trees);

    while (m_Running) {
        Request request;
        m_Requests.WaitAndPop(request);
        if (!m_Running) break;

        Result result;
        result.cx = request.cx; result.cy = request.cy; result.cz = request.cz;

        // Skip requests the player has already flown away from
        int dist = std::max(std::abs(request.cx - m_WorkerPlayerCx.load()), std::abs(request.cz - m_WorkerPlayerCz.load()));
        if (dist > m_CancelRadius.load()) {
            result.cancelled = true;
        } else {
            generator.GenerateChunk(request.cx, request.cy, request.cz, result.data);
            // Done here so the main thread only has to upload
            if (!GpuChunkCache::ComputeBrickMask(result.data, result.brickMask)) std::vector<uint8_t>().swap(result.data);
        }
        m_Results.Push(std::move(result));
    }
}

int ChunkStreamer::DistanceXZ(int cx, int cz) const {
    return std::max(std::abs(cx - m_PlayerChunk.x), std::abs(cz - m_PlayerChunk.z));
}

// Whether the current windows want this chunk at all (CPU copy or GPU upload)
bool ChunkStreamer::IsWanted(int cx, int cz) const {
    int dist = DistanceXZ(cx, cz);
    return dist <= CPU_RADIUS || dist <= m_RenderDistance;
}

void ChunkStreamer::RequestChunk(int cx, int cy, int cz) {
    if (m_InFlight.insert(ChunkKey(cx, cy, cz)).second) {
        m_Requests.Push({ cx, cy, cz });
    }
}

bool ChunkStreamer::SetPlayerChunk(glm::ivec3 chunk) {
    bool movedColumn = chunk.x != m_PlayerChunk.x || chunk.z != m_PlayerChunk.z;
    m_PlayerChunk = chunk;
    return movedColumn;
}

void ChunkStreamer::Integrate(Result& result) {
    uint64_t key = ChunkKey(result.cx, result.cy, result.cz);
    m_InFlight.erase(key);

    if (result.cancelled) {
        // The player may have come back since the worker skipped it
        if (IsWanted(result.cx, result.cz)) RequestChunk(result.cx, result.cy, result.cz);
        return;
    }

    int dist = DistanceXZ(result.cx, result.cz);
    Chunk* cpuChunk = m_World.FindChunk(result.cx, result.cy, result.cz);

    // GPU: upload if in the render distance; an edited CPU copy always wins over generated data
    bool wantGpu = dist <= m_RenderDistance && !m_Cache.IsResident(key) && !m_Cache.IsKnownEmpty(key);
    if (wantGpu) {
        glm::ivec3 coord(result.cx, result.cy, result.cz);
        if (cpuChunk && cpuChunk->isModified) {
            m_Cache.MakeResident(key, coord, cpuChunk->data, nullptr);
        } else {
            m_Cache.MakeResident(key, coord, result.data, result.brickMask);
        }
    }

    // CPU: keep voxel data only close to the player
    if (!cpuChunk && dist <= CPU_RADIUS) {
        Chunk* chunk = m_World.GetOrCreateChunk(result.cx, result.cy, result.cz);
        chunk->data = std::move(result.data);
    }
}

void ChunkStreamer::IntegrateResults(double frameStartTime, double budgetSeconds) {
    Result result;
    while (glfwGetTime() - frameStartTime < budgetSeconds && m_Results.TryPop(result)) {
        Integrate(result);
    }
}

void ChunkStreamer::WaitForColumn(int cx, int cz) {
    auto columnReady = [&]() {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) {
            if (!m_World.FindChunk(cx, cy, cz)) return false;
        }
        return true;
    };
    while (!columnReady()) {
        Result result;
        m_Results.WaitAndPop(result);
        Integrate(result);
    }
}

void ChunkStreamer::RefreshChunk(uint64_t key) {
    int cx, cy, cz;
    UnpackChunkKey(key, cx, cy, cz);
    Chunk* chunk = m_World.FindChunk(cx, cy, cz);
    if (!chunk) return;
    if (m_Cache.IsResident(key) || DistanceXZ(chunk->position.x, chunk->position.z) <= m_RenderDistance) {
        m_Cache.MakeResident(key, chunk->position, chunk->data, nullptr);
    }
}

bool ChunkStreamer::Window::Contains(int cx, int cz) const {
    return radius >= 0 && std::abs(cx - center.x) <= radius && std::abs(cz - center.y) <= radius;
}

// Columns inside `to` but not inside `from`, nearest to `to`'s center first
std::vector<glm::ivec2> ChunkStreamer::ColumnsEntering(const Window& from, const Window& to) {
    std::vector<glm::ivec2> columns;
    if (to.radius < 0) return columns;
    for (int dz = -to.radius; dz <= to.radius; dz++) {
        for (int dx = -to.radius; dx <= to.radius; dx++) {
            int cx = to.center.x + dx;
            int cz = to.center.y + dz;
            if (!from.Contains(cx, cz)) columns.push_back(glm::ivec2(cx, cz));
        }
    }
    std::sort(columns.begin(), columns.end(), [&to](const glm::ivec2& a, const glm::ivec2& b) {
        glm::ivec2 da = a - to.center, db = b - to.center;
        return da.x * da.x + da.y * da.y < db.x * db.x + db.y * db.y;
    });
    return columns;
}

void ChunkStreamer::UpdateWindows() {
    m_WorkerPlayerCx = m_PlayerChunk.x;
    m_WorkerPlayerCz = m_PlayerChunk.z;
    m_CancelRadius = std::max(m_RenderDistance + GPU_EVICT_MARGIN, CPU_KEEP_RADIUS);

    glm::ivec2 center(m_PlayerChunk.x, m_PlayerChunk.z);
    Window gpuLoad{ center, m_RenderDistance };
    Window gpuKeep{ center, m_RenderDistance + GPU_EVICT_MARGIN };
    Window cpuLoad{ center, CPU_RADIUS };
    Window cpuKeep{ center, CPU_KEEP_RADIUS };

    // 1. Evict GPU chunks that left the keep window
    for (const glm::ivec2& column : ColumnsEntering(gpuKeep, m_GpuKeepWindow)) {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) m_Cache.Forget(ChunkKey(column.x, cy, column.y));
    }

    // 2. Free CPU chunks that left the keep window; edited chunks are kept forever
    for (const glm::ivec2& column : ColumnsEntering(cpuKeep, m_CpuKeepWindow)) {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) m_World.FreeChunkIfUnmodified(column.x, cy, column.y);
    }

    // 3. Request CPU data for columns entering the CPU window (nearest first)
    for (const glm::ivec2& column : ColumnsEntering(m_CpuLoadWindow, cpuLoad)) {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) {
            if (!m_World.FindChunk(column.x, cy, column.y)) RequestChunk(column.x, cy, column.y);
        }
    }

    // 4. Upload or request chunks entering the render distance (nearest first)
    for (const glm::ivec2& column : ColumnsEntering(m_GpuLoadWindow, gpuLoad)) {
        for (int cy = 0; cy < CHUNK_LAYERS; cy++) {
            uint64_t key = ChunkKey(column.x, cy, column.y);
            if (m_Cache.IsResident(key) || m_Cache.IsKnownEmpty(key)) continue;
            if (Chunk* chunk = m_World.FindChunk(column.x, cy, column.y)) {
                m_Cache.MakeResident(key, chunk->position, chunk->data, nullptr);
            } else {
                RequestChunk(column.x, cy, column.y);
            }
        }
    }

    m_GpuLoadWindow = gpuLoad;
    m_GpuKeepWindow = gpuKeep;
    m_CpuLoadWindow = cpuLoad;
    m_CpuKeepWindow = cpuKeep;
}

void ChunkStreamer::SetRenderDistance(int chunks) {
    chunks = std::clamp(chunks, MIN_RENDER_DISTANCE, MAX_RENDER_DISTANCE);
    if (chunks == m_RenderDistance) return;
    m_RenderDistance = chunks;
    UpdateWindows();
}
