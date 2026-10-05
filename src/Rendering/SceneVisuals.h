#pragma once

#include <glm/glm.hpp>

// Gameplay effects the renderer draws into the world, produced by PlayerTools each frame.
// All positions are in world units.
struct SceneVisuals {
    static constexpr int MAX_FLARES = 8; // Matches flares[8] in shaders/include/scene.glsl

    struct Beam { // Dissolver / tether beam
        bool active = false;
        glm::vec3 start = glm::vec3(0.0f);
        glm::vec3 end = glm::vec3(0.0f);
        glm::vec3 color = glm::vec3(1.0f, 0.0f, 0.0f);
    };

    struct Clump { // Voxels held by the gravity tether, drawn as a glowing sphere
        bool active = false;
        bool isArtifact = false;
        glm::vec3 position = glm::vec3(0.0f);
        float radius = 1.5f; // Voxels
    };

    struct FlareLight {
        glm::vec3 position;
        glm::vec3 color;
        float intensity;
    };

    Beam beam;
    Clump clump;
    FlareLight flares[MAX_FLARES];
    int flareCount = 0;
};
