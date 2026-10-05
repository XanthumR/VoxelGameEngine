// Camera rays for the render passes

#include "include/scene.glsl"

struct Ray {
    vec3 origin;
    vec3 dir;
};

Ray cameraRay(ivec2 pixel, ivec2 dims) {
    vec2 pxNDC = vec2(pixel) / vec2(dims) * 2.0 - 1.0;
    vec4 target = inverseProj * vec4(pxNDC, 1.0, 1.0);
    vec3 rayDirWorld = normalize((inverseView * vec4(normalize(target.xyz / target.w), 0.0)).xyz);
    return Ray(cameraPos, rayDirWorld);
}
