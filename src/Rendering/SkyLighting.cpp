#include "Rendering/SkyLighting.h"

#include <cmath>

SkyLighting SkyLighting::At(float time) {
    const float PI = 3.14159265f;
    float angle = (time / DAY_DURATION) * 2.0f * PI;

    SkyLighting sky;
    sky.sunDir = glm::normalize(glm::vec3(cos(angle), sin(angle), 0.4f));
    sky.moonDir = -sky.sunDir;

    float t = sky.sunDir.y;
    if (t > 0.0f) {
        // Daytime: the sun is the light
        sky.skyColor = glm::mix(glm::vec3(0.8f, 0.4f, 0.3f), glm::vec3(0.4f, 0.7f, 1.0f), t);
        sky.lightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.3f), glm::vec3(1.0f, 0.95f, 0.85f), t);
        sky.lightDir = sky.sunDir;
        sky.ambient = glm::mix(0.15f, 0.25f, t);
    } else {
        // Nighttime: the moon is the light
        float nightFactor = glm::clamp(-t * 4.0f, 0.0f, 1.0f);
        sky.skyColor = glm::mix(glm::vec3(0.8f, 0.4f, 0.3f), glm::vec3(0.01f, 0.02f, 0.05f), nightFactor);
        sky.lightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.3f), glm::vec3(0.08f, 0.12f, 0.20f), nightFactor);
        sky.lightDir = sky.moonDir;
        sky.ambient = glm::mix(0.15f, 0.05f, nightFactor);
    }
    return sky;
}
