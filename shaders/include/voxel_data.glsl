// Voxel data on the GPU (GpuChunkCache): reading voxels through the page table, and skipping
// empty chunks and bricks while ray marching.

layout(binding = 1) uniform isampler3D pageTable;
// Per 32x32 chunk column (wrapped like the page table): 1 + the highest voxel y that may be solid
// there, 0 for none. Rays skip the air above it in one step (skipAboveColumn).
layout(binding = 0) uniform usampler2D columnTops;

// Chunk voxel data is split over up to 4 pool textures, each a 64 x 64 x N grid of 32^3 slots,
// with a brick mask beside each (pool downscaled 8x: non-zero if the 8^3 brick has a solid voxel).
// Global slot indices run through the pools in order; poolBase holds the first slot of pools 1..3.
layout(binding = 2) uniform sampler3D chunkPool0;
layout(binding = 3) uniform sampler3D brickMask0;
layout(binding = 4) uniform sampler3D chunkPool1;
layout(binding = 5) uniform sampler3D brickMask1;
layout(binding = 6) uniform sampler3D chunkPool2;
layout(binding = 7) uniform sampler3D brickMask2;
layout(binding = 8) uniform sampler3D chunkPool3;
layout(binding = 9) uniform sampler3D brickMask3;
uniform ivec3 poolBase;
const int PAGE_TABLE_WRAP = 512;

// Chunk coordinate of a voxel (arithmetic shift == floor division by 32, also for negatives)
ivec3 chunkOf(ivec3 ipos) {
    return ipos >> 5;
}

// Returns the pool slot holding this chunk, or -1 when it has none.
// No slot means every voxel in the chunk reads as air (empty or not uploaded).
int getChunkSlot(ivec3 chunkCoord) {
    // Horizontal coordinates wrap toroidally; Y remains bounded [0, 3]
    if (chunkCoord.y < 0 || chunkCoord.y >= 4) return -1;

    int ptX = chunkCoord.x % PAGE_TABLE_WRAP; if (ptX < 0) ptX += PAGE_TABLE_WRAP;
    int ptY = chunkCoord.y;
    int ptZ = chunkCoord.z % PAGE_TABLE_WRAP; if (ptZ < 0) ptZ += PAGE_TABLE_WRAP;

    ivec4 pageData = texelFetch(pageTable, ivec3(ptX, ptY, ptZ), 0);

    int slotIndex = pageData.x - 1;
    ivec3 storedCoord = pageData.yzw;

    // Validate that the slot actually contains data for our chunk (and not a wrapped old chunk)
    // Component-wise comparison avoids driver-dependent bvec3-to-boolean casting issues
    if (slotIndex < 0 ||
        storedCoord.x != chunkCoord.x ||
        storedCoord.y != chunkCoord.y ||
        storedCoord.z != chunkCoord.z) {
        return -1;
    }
    return slotIndex;
}

// Splits a global slot index into its pool and the slot's grid position inside that pool
int locateSlot(int slotIndex, out ivec3 slotPos) {
    int pool = slotIndex >= poolBase.z ? 3 : slotIndex >= poolBase.y ? 2 : slotIndex >= poolBase.x ? 1 : 0;
    int local = slotIndex - (pool == 0 ? 0 : poolBase[pool - 1]);
    slotPos = ivec3(local % 64, (local / 64) % 64, local / 4096);
    return pool;
}

// Reads a voxel from a chunk already resolved to its pool slot
float sampleSlot(int slotIndex, ivec3 ipos) {
    ivec3 slotPos;
    int pool = locateSlot(slotIndex, slotPos);
    ivec3 coord = slotPos * 32 + (ipos & 31);
    if (pool == 0) return texelFetch(chunkPool0, coord, 0).r;
    if (pool == 1) return texelFetch(chunkPool1, coord, 0).r;
    if (pool == 2) return texelFetch(chunkPool2, coord, 0).r;
    return texelFetch(chunkPool3, coord, 0).r;
}

float getVoxelVal(ivec3 ipos) {
    int slotIndex = getChunkSlot(chunkOf(ipos));
    return slotIndex < 0 ? 0.0 : sampleSlot(slotIndex, ipos);
}

// True if the 8^3 brick containing ipos (inside the chunk in slotIndex) has no solid voxel
bool isBrickEmpty(int slotIndex, ivec3 ipos) {
    ivec3 slotPos;
    int pool = locateSlot(slotIndex, slotPos);
    ivec3 coord = slotPos * 4 + ((ipos & 31) >> 3);
    float mask;
    if (pool == 0) mask = texelFetch(brickMask0, coord, 0).r;
    else if (pool == 1) mask = texelFetch(brickMask1, coord, 0).r;
    else if (pool == 2) mask = texelFetch(brickMask2, coord, 0).r;
    else mask = texelFetch(brickMask3, coord, 0).r;
    return mask == 0.0;
}

// The highest voxel y that may be solid in a chunk column, -1 for none
int columnTop(ivec2 chunkXZ) {
    int ptX = chunkXZ.x % PAGE_TABLE_WRAP; if (ptX < 0) ptX += PAGE_TABLE_WRAP;
    int ptZ = chunkXZ.y % PAGE_TABLE_WRAP; if (ptZ < 0) ptZ += PAGE_TABLE_WRAP;
    return int(texelFetch(columnTops, ivec2(ptX, ptZ), 0).r) - 1;
}

// Advances DDA state (mapPos, sideDist) to the first voxel past the box boxMin..boxMax
// (inclusive, containing mapPos), in one jump instead of many single steps. Uses the same
// per-axis crossing arithmetic as the DDA itself, so the result is exactly where stepping
// would arrive.
void skipBox(ivec3 boxMin, ivec3 boxMax, ivec3 stepDir, vec3 deltaDist, inout ivec3 mapPos, inout vec3 sideDist, inout vec3 normal) {
    // Ray distance at which we leave the box through each axis' far wall
    ivec3 remaining;
    float tExit = 1e30;
    int exitAxis = 0;
    for (int a = 0; a < 3; a++) {
        remaining[a] = stepDir[a] > 0 ? (boxMax[a] - mapPos[a]) : (mapPos[a] - boxMin[a]);
        float tAxis = stepDir[a] != 0 ? sideDist[a] + float(remaining[a]) * deltaDist[a] : 1e30;
        if (tAxis < tExit) { tExit = tAxis; exitAxis = a; }
    }

    // Apply every wall crossing that happens before tExit
    for (int a = 0; a < 3; a++) {
        if (stepDir[a] == 0) continue;
        int crossings;
        if (a == exitAxis) {
            crossings = remaining[a] + 1; // out of the cell
        } else if (sideDist[a] < tExit) {
            crossings = min(int(ceil((tExit - sideDist[a]) / deltaDist[a])), remaining[a]);
        } else {
            crossings = 0;
        }
        mapPos[a] += crossings * stepDir[a];
        sideDist[a] += float(crossings) * deltaDist[a];
    }

    normal = vec3(0.0);
    normal[exitAxis] = float(-stepDir[exitAxis]);
}

// The same for the aligned cell of cellSize^3 voxels around mapPos (32 = chunk, 8 = brick)
void skipCell(int cellSize, ivec3 stepDir, vec3 deltaDist, inout ivec3 mapPos, inout vec3 sideDist, inout vec3 normal) {
    ivec3 cellMin = mapPos & ~(cellSize - 1); // floor to cell (cellSize is a power of two)
    skipBox(cellMin, cellMin + cellSize - 1, stepDir, deltaDist, mapPos, sideDist, normal);
}

// Above floorY in a chunk column there is only air (floorY: at least the column's top): skips
// the ray out of that air box, to just above the floor or into the next column. False if mapPos
// is not above the floor.
bool skipAboveColumn(int floorY, ivec3 stepDir, vec3 deltaDist, inout ivec3 mapPos, inout vec3 sideDist, inout vec3 normal) {
    if (mapPos.y <= floorY) return false;
    ivec3 boxMin = ivec3((mapPos.x >> 5) << 5, floorY + 1, (mapPos.z >> 5) << 5);
    skipBox(boxMin, ivec3(boxMin.x + 31, 127, boxMin.z + 31), stepDir, deltaDist, mapPos, sideDist, normal);
    return true;
}
