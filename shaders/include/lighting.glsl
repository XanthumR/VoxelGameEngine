// Shadow rays toward the sun/moon (and flares)

#include "include/scene.glsl"
#include "include/voxel_data.glsl"

// Marches toward the light; 0.3 if anything solid is in the way, 1.0 otherwise
float getShadow(vec3 worldStart, vec3 lightDir) {
    vec3 rayPos = worldStart * float(dimX);
    ivec3 mapPos = ivec3(floor(rayPos));

    vec3 deltaDist = abs(1.0 / lightDir);
    ivec3 stepDir = ivec3(sign(lightDir));
    vec3 sideDist = (sign(lightDir) * (vec3(mapPos) - rayPos) + (sign(lightDir) * 0.5) + 0.5) * deltaDist;

    ivec3 cachedChunk = ivec3(0x7fffffff);
    int cachedSlot = -1;
    ivec3 cachedBrick = ivec3(0x7fffffff);
    bool cachedBrickEmpty = false;
    vec3 unusedNormal;

    for (int i = 0; i < 200; i++) {
        // Page table is only re-read when the ray enters a new chunk
        ivec3 chunkCoord = chunkOf(mapPos);
        if (any(notEqual(chunkCoord, cachedChunk))) {
            cachedChunk = chunkCoord;
            cachedSlot = getChunkSlot(chunkCoord);
        }

        // Empty chunk: nothing can block the light inside it, jump straight across
        if (cachedSlot < 0) {
            skipCell(32, stepDir, deltaDist, mapPos, sideDist, unusedNormal);
            if (mapPos.y < 0 || mapPos.y >= dimY) break;
            continue;
        }

        // Empty 8^3 brick inside a non-empty chunk: same idea at a finer level
        ivec3 brickCoord = mapPos >> 3;
        if (any(notEqual(brickCoord, cachedBrick))) {
            cachedBrick = brickCoord;
            cachedBrickEmpty = isBrickEmpty(cachedSlot, mapPos);
        }
        if (cachedBrickEmpty) {
            skipCell(8, stepDir, deltaDist, mapPos, sideDist, unusedNormal);
            if (mapPos.y < 0 || mapPos.y >= dimY) break;
            continue;
        }

        // Water does not cast shadows (it is a surface, and would darken lower wave steps)
        float blocker = sampleSlot(cachedSlot, mapPos);
        if (blocker > 0.0 && int(blocker * 255.0 + 0.5) != WATER_ID) return 0.3;

        if (sideDist.x < sideDist.y && sideDist.x < sideDist.z) {
            sideDist.x += deltaDist.x; mapPos.x += stepDir.x;
        } else if (sideDist.y < sideDist.z) {
            sideDist.y += deltaDist.y; mapPos.y += stepDir.y;
        } else {
            sideDist.z += deltaDist.z; mapPos.z += stepDir.z;
        }

        // Exit if we leave the vertical bounds of the world
        if (mapPos.y < 0 || mapPos.y >= dimY) break;
    }
    return 1.0;
}

// Soft shadow: 4 jittered rays toward the light. Depends only on the voxel and face, not the
// pixel.
float softShadow(ivec3 mapPos, vec3 normal) {
    // A face turned away from the light is fully shadowed (its rays would only hit
    // its own block), so skip the rays. Diffuse is zero there anyway.
    if (dot(normal, lightDir) <= 0.0) return 0.3;

    float shadowAccum = 0.0;
    int shadowSamples = 4;   // Fixed to 4 for the offsets array
    float sunSize = 0.060;

    vec3 upGuide = abs(lightDir.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 sunRight = normalize(cross(upGuide, lightDir));
    vec3 sunUp = cross(lightDir, sunRight);

    vec2 offsets[4] = vec2[](
        vec2(-0.5, -0.5), vec2( 0.5, -0.5),
        vec2(-0.5,  0.5), vec2( 0.5,  0.5)
    );

    vec3 shadowStart = (vec3(mapPos) + 0.5 + normal * 0.6) / float(dimX);
    for (int s = 0; s < shadowSamples; s++) {
        vec2 jitter = offsets[s];
        vec3 jitteredLightDir = normalize(lightDir + (sunRight * jitter.x * sunSize) + (sunUp * jitter.y * sunSize));
        shadowAccum += getShadow(shadowStart, jitteredLightDir);
    }

    return shadowAccum / float(shadowSamples);
}
