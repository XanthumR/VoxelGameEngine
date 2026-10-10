#pragma once

#include "Gameplay/Figure.h"
#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class BuildingModelLibrary;

// Smoke from chimneys. Purely visual (not part of the deterministic simulation): every building
// whose model has smoke emitters (palette index 100 in its .vox) puffs while it is in use: houses
// with residents, producers while they work. A puff rises, drifts with the wind, grows and thins out, then
// disappears; it is drawn as a Figure::PUFF.
class SmokeSystem {
public:
    static constexpr int MAX_PUFFS = 1536;
    static constexpr int MAX_EMITTERS_PER_BUILDING = 4;
    static constexpr float PUFF_INTERVAL = 0.7f; // Seconds between puffs of one chimney
    static constexpr float LIFETIME = 5.0f;      // Seconds a puff lasts
    static constexpr float RISE_SPEED = 4.0f;    // Voxels per second
    static constexpr float DRIFT_SPEED = 2.5f;   // Voxels per second, with the wind
    static constexpr float DUST_RATE = 8.0f;     // Puffs per second of a building going up

    SmokeSystem();

    // focusColumn and maxDistance (voxels): only chimneys near the camera's focus smoke
    void Update(float deltaTime, const GameObjectRegistry& objects, const BuildingModelLibrary& models, glm::vec2 focusColumn, float maxDistance);
    void AppendFigures(std::vector<Figure>& out) const;

    // Dust of a building going up: now and then a puff at a random point on the edge of the
    // footprint (min to max, voxel columns), at height y
    void Dust(float deltaTime, glm::vec2 min, glm::vec2 max, float y);
    size_t PuffCount() const { return m_Puffs.size(); }

    // Whether a building is lived in or working, so its chimneys smoke
    static bool InUse(const GameObjectRegistry& objects, GameObjectId id);
    static bool BlackSmoke(uint16_t type); // Heavy industry: coal smoke
    void SetAlwaysInUse(bool always) { m_AlwaysInUse = always; } // Every chimney smokes (--showcase)

private:
    struct Puff {
        glm::vec3 position; // Voxels
        float age;          // Seconds
        uint8_t seed;
        bool dark = false;  // Coal smoke (heavy industry)
    };

    uint32_t Random();
    bool m_AlwaysInUse = false;

    std::vector<Puff> m_Puffs;
    std::vector<float> m_Timers; // Per object slot and emitter: seconds until the next puff, < 0 = not started
    uint32_t m_RandomState = 0x2545F491u;
};
