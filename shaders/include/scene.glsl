// Per-frame scene uniforms, set by VoxelRenderer for every render pass (and by ShoreMap), and
// the block IDs the shaders treat specially (same values as src/World/BlockTypes.h).

uniform int renderDistanceVoxels; // Horizontal ray cutoff, set from the render distance slider
uniform int seaLevel;             // Ocean surface height (top water voxel when calm)
uniform vec3 beaconPos;           // Spawn beacon base, world units
uniform int chunkViewerEnabled;
uniform int lightVisualizerEnabled;
uniform int halfResShadows;       // 1 = use the shadow pass results, 0 = every pixel traces its own
uniform ivec2 renderSize;         // Full-resolution render target size

uniform vec3 cameraPos;
uniform mat4 inverseView;
uniform mat4 inverseProj;

uniform int dimX; // World -> voxel scale (VOXELS_PER_UNIT)
uniform int dimY; // World height in voxels
uniform int dimZ;
uniform float time;

// Day-night cycle (SkyLighting)
uniform vec3 sunDir;
uniform vec3 moonDir;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 skyColor;
uniform float ambient;

// Player tools: dissolver / tether beam
uniform int laserBeamActive;
uniform vec3 laserBeamStart;
uniform vec3 laserBeamEnd;
uniform vec3 laserBeamColor;

// Player tools: flares
struct Flare {
    vec3 pos;
    vec3 color;
    float intensity;
};
uniform int numFlares;
uniform Flare flares[8];

// Player tools: clump held by the gravity tether
uniform int heldClumpActive;
uniform vec3 heldClumpPos;
uniform float heldClumpRadius;
uniform int heldClumpIsArtifact;

const int SAND_ID = 4;
const int WATER_ID = 35;
