#pragma once

#include "Gameplay/Figure.h"
#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class BuildingModelLibrary;

// Smoke from chimneys. Purely visual (not part of the deterministic simulation): every building
// whose model has smoke emitters (palette index 100 in its .vox) puffs while it is in use, houses
// with residents and producers. A puff rises, drifts with the wind, grows and thins out, then
// disappears; it is drawn as a Figure::PUFF.
class SmokeSystem {
public:
    static constexpr int MAX_PUFFS = 1536;
    static constexpr int MAX_EMITTERS_PER_BUILDING = 4;
    static constexpr float PUFF_INTERVAL = 0.7f; // Seconds between puffs of one chimney
    static constexpr float LIFETIME = 5.0f;      // Seconds a puff lasts
    static constexpr float RISE_SPEED = 4.0f;    // Voxels per second
    static constexpr float DRIFT_SPEED = 2.5f;   // Voxels per second, with the wind

    SmokeSystem();

    // focusColumn and maxDistance (voxels): only chimneys near the camera's focus smoke
    void Update(float deltaTime, const GameObjectRegistry& objects, const BuildingModelLibrary& models, glm::vec2 focusColumn, float maxDistance);
    void AppendFigures(std::vector<Figure>& out) const;
    size_t PuffCount() const { return m_Puffs.size(); }

    // Whether a building is lived in or working, so its chimneys smoke
    static bool InUse(const GameObjectRegistry& objects, GameObjectId id);

private:
    struct Puff {
        glm::vec3 position; // Voxels
        float age;          // Seconds
        uint8_t seed;
    };

    uint32_t Random();

    std::vector<Puff> m_Puffs;
    std::vector<float> m_Timers; // Per object slot and emitter: seconds until the next puff, < 0 = not started
    uint32_t m_RandomState = 0x2545F491u;
};
