// Ocean surface: FFT waves (OceanSimulation), the shore map (ShoreMap), and the breakers, swash
// and wave height built from them. The wave height moves the sea surface by up to MAX_WAVE
// voxels while tracing, so the whole infinite ocean waves without writing any voxel data.

#include "include/scene.glsl"

// FFT ocean: one layer per cascade, sampled at worldXZ / tile size
layout(binding = 10) uniform sampler2DArray oceanDisplacement; // Dx, Dz, height, dDx/dz
layout(binding = 11) uniform sampler2DArray oceanSlope;        // dh/dx, dh/dz, dDx/dx, dDz/dz
uniform vec3 oceanTileSizes;
uniform float oceanChoppiness;

// Shore map: per voxel column around the player. x = signed distance to the coast
// (+ over water, - on land), y = water depth, zw = direction inland
layout(binding = 12) uniform sampler2D shoreMap;
uniform ivec2 shoreOrigin;
const int SHORE_MAP_SIZE = 1024;

const int MAX_WAVE = 2; // seaLevel - 2 .. seaLevel + 2 stays inside one 8^3 brick (y 40-47)

vec4 sampleOcean(sampler2DArray fields, vec2 xz) {
    return texture(fields, vec3(xz / oceanTileSizes.x, 0.0))
         + texture(fields, vec3(xz / oceanTileSizes.y, 1.0))
         + texture(fields, vec3(xz / oceanTileSizes.z, 2.0));
}

// Slopes for shading, with the smaller cascades boosted: short waves carry little energy, so
// physically correct normals look glassy from a distance. Geometry is not affected.
vec4 sampleOceanSlopeForShading(vec2 xz) {
    return texture(oceanSlope, vec3(xz / oceanTileSizes.x, 0.0))
         + texture(oceanSlope, vec3(xz / oceanTileSizes.y, 1.0)) * 2.5
         + texture(oceanSlope, vec3(xz / oceanTileSizes.z, 2.0)) * 5.0;
}

// --- Shore Interaction ---

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x),
               mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x), u.y);
}

// Outside the map counts as open sea
vec4 sampleShore(vec2 xz) {
    vec2 uv = (xz - vec2(shoreOrigin)) / float(SHORE_MAP_SIZE);
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(1000.0, 32.0, 0.0, 0.0);
    return texture(shoreMap, uv);
}

const float SURF_WAVENUMBER = 0.35; // Breaker spacing ~18 voxels
const float SURF_FREQUENCY = 1.6;   // Breakers move inland at ~4.6 voxels/s
const float SURF_ZONE = 48.0;       // Breakers start this far out

// Breaker strength varies slowly along the coast, so the surf is not uniform
float surfStrength(vec2 xz) {
    return 0.55 + 0.45 * valueNoise(xz * 0.03 + vec2(time * 0.05, -time * 0.04));
}

// Breakers rolling toward the coast: the phase grows with distance from land, so crests move
// inland and follow every bay and headland. They grow as the water shallows (shoaling) and
// collapse right at the waterline.
float surfHeight(float distanceToLand, float strength) {
    if (distanceToLand <= 0.0 || distanceToLand > SURF_ZONE) return 0.0;
    float crest = pow(0.5 + 0.5 * sin(distanceToLand * SURF_WAVENUMBER + time * SURF_FREQUENCY), 4.0);
    float shoaling = smoothstep(SURF_ZONE, 10.0, distanceToLand) * smoothstep(0.0, 3.0, distanceToLand);
    return 2.2 * crest * shoaling * strength;
}

// How far up the beach (voxels inland) the current wave has run
float swashReach(float strength) {
    return 5.0 * pow(0.5 + 0.5 * sin(time * SURF_FREQUENCY), 4.0) * strength;
}

// The FFT sea fades in shallow water and near the coast, where the breakers take over
float openSeaFactor(vec4 shore) {
    return clamp(shore.y / 10.0, 0.2, 1.0) * smoothstep(0.0, 10.0, shore.x);
}

float oceanHeight(vec2 xz) {
    vec4 shore = sampleShore(xz);
    return sampleOcean(oceanDisplacement, xz).z * openSeaFactor(shore) + surfHeight(shore.x, surfStrength(xz));
}

// Whole-voxel offset of the sea surface at a column (-MAX_WAVE..MAX_WAVE)
int seaWave(ivec2 xz) {
    return int(clamp(round(oceanHeight(vec2(xz) + 0.5)), -float(MAX_WAVE), float(MAX_WAVE)));
}
