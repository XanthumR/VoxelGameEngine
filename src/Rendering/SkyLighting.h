#pragma once

#include <glm/glm.hpp>

// Sun, moon, sky color and ambient light for a moment of the day-night cycle
struct SkyLighting {
    static constexpr float DAY_DURATION = 600.0f; // Seconds per full day-night cycle

    glm::vec3 sunDir;
    glm::vec3 moonDir;
    glm::vec3 lightDir;   // Sun by day, moon by night
    glm::vec3 lightColor;
    glm::vec3 skyColor;
    float ambient;

    static SkyLighting At(float time);
};
