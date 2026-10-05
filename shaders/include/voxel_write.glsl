// Writing voxels straight into the GPU chunk pools, for animation passes that change the world
// between CPU uploads (grass/grass_animate.comp, people/walkers.comp). The pools and brick masks
// are bound as images by GpuChunkCache::BindAsImages, the page table on texture unit 1.

// Chunk pools as images (unit = pool index) and their brick masks (unit = 4 + pool index)
layout(r8, binding = 0) uniform image3D chunkPool0;
layout(r8, binding = 1) uniform image3D chunkPool1;
layout(r8, binding = 2) uniform image3D chunkPool2;
layout(r8, binding = 3) uniform image3D chunkPool3;
layout(r8, binding = 4) uniform writeonly image3D brickMask0;
layout(r8, binding = 5) uniform writeonly image3D brickMask1;
layout(r8, binding = 6) uniform writeonly image3D brickMask2;
layout(r8, binding = 7) uniform writeonly image3D brickMask3;
layout(binding = 1) uniform isampler3D pageTable;
uniform ivec3 poolBase; // First global slot of pools 1..3 (same as include/voxel_data.glsl)
const int PAGE_TABLE_WRAP = 512;

// Resolves a voxel to its pool and texel. Returns -1 if its chunk is not on the GPU.
int locateVoxel(ivec3 p, out ivec3 coord, out ivec3 brickCoord) {
    ivec3 chunkCoord = p >> 5;
    if (chunkCoord.y < 0 || chunkCoord.y >= 4) return -1;

    int ptX = chunkCoord.x % PAGE_TABLE_WRAP; if (ptX < 0) ptX += PAGE_TABLE_WRAP;
    int ptZ = chunkCoord.z % PAGE_TABLE_WRAP; if (ptZ < 0) ptZ += PAGE_TABLE_WRAP;
    ivec4 pageData = texelFetch(pageTable, ivec3(ptX, chunkCoord.y, ptZ), 0);

    int slotIndex = pageData.x - 1;
    // Validate that the slot holds this chunk (not a wrapped neighbour); component-wise for driver safety
    if (slotIndex < 0 || pageData.y != chunkCoord.x || pageData.z != chunkCoord.y || pageData.w != chunkCoord.z) return -1;

    int pool = slotIndex >= poolBase.z ? 3 : slotIndex >= poolBase.y ? 2 : slotIndex >= poolBase.x ? 1 : 0;
    int local = slotIndex - (pool == 0 ? 0 : poolBase[pool - 1]);
    ivec3 slotPos = ivec3(local % 64, (local / 64) % 64, local / 4096);
    coord = slotPos * 32 + (p & 31);
    brickCoord = slotPos * 4 + ((p & 31) >> 3);
    return pool;
}

float loadVoxel(int pool, ivec3 c) {
    if (pool == 0) return imageLoad(chunkPool0, c).r;
    if (pool == 1) return imageLoad(chunkPool1, c).r;
    if (pool == 2) return imageLoad(chunkPool2, c).r;
    return imageLoad(chunkPool3, c).r;
}

void storeVoxel(int pool, ivec3 c, float v) {
    if (pool == 0) imageStore(chunkPool0, c, vec4(v));
    else if (pool == 1) imageStore(chunkPool1, c, vec4(v));
    else if (pool == 2) imageStore(chunkPool2, c, vec4(v));
    else imageStore(chunkPool3, c, vec4(v));
}

// A voxel in a brick the CPU thought was empty: mark it so the ray marcher does not skip it
void markBrick(int pool, ivec3 b) {
    if (pool == 0) imageStore(brickMask0, b, vec4(1.0));
    else if (pool == 1) imageStore(brickMask1, b, vec4(1.0));
    else if (pool == 2) imageStore(brickMask2, b, vec4(1.0));
    else imageStore(brickMask3, b, vec4(1.0));
}

// Block ID stored in a voxel value
int voxelId(float v) {
    return int(v * 255.0 + 0.5);
}
