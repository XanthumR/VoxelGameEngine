// G-buffer written by the trace pass and read by the shadow and shade passes.
// Each texel: xyz = hit voxel, w = packed info:
//   bits 0-2: normal index (0..5, NO_HIT = no hit), bits 3-10: voxel ID, bits 11-18: chunk grid factor

const int NO_HIT = 7;

int normalToIndex(vec3 n) {
    if (n.x > 0.5) return 0;
    if (n.x < -0.5) return 1;
    if (n.y > 0.5) return 2;
    if (n.y < -0.5) return 3;
    if (n.z > 0.5) return 4;
    return 5;
}

vec3 indexToNormal(int i) {
    vec3 n = vec3(0.0);
    n[i >> 1] = (i & 1) == 0 ? 1.0 : -1.0;
    return n;
}

int packHitInfo(int normalIndex, int voxelID, float gridFactor) {
    return normalIndex | (voxelID << 3) | (int(clamp(gridFactor, 0.0, 1.0) * 255.0 + 0.5) << 11);
}
