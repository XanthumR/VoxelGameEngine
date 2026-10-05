#pragma once

#include "Rendering/SceneVisuals.h"

#include <glm/glm.hpp>

#include <vector>

struct GLFWwindow;
class Player;
class VoxelWorld;
class WorldEditor;

// The player's tools and the objects they create:
//  - Matter dissolver (left click): digs a sphere where the crosshair points
//  - Gravity tether (right click): picks up a clump of voxels and throws it on release; an
//    artifact carried to the spawn beacon is collected
//  - Paint mode (P, right click): places the selected block
//  - Flares (F): glowing projectiles that stick to surfaces and light them
//  - Artifact scanner: distance to the nearest artifact
class PlayerTools {
public:
    static constexpr float REACH_DISTANCE = 10.0f; // World units

    PlayerTools(VoxelWorld& world, WorldEditor& editor);

    void SetBeaconColumn(glm::ivec2 column) { m_BeaconColumn = column; }

    void HandleFlareKey(GLFWwindow* window, const Player& player);
    void HandleBlockSelectKeys(GLFWwindow* window);
    void HandleMouse(GLFWwindow* window, float deltaTime, const Player& player);
    void UpdateProjectiles(float deltaTime);
    void UpdateScanner(float deltaTime, const Player& player);

    // Green on terrain, magenta on an artifact, white otherwise
    glm::vec3 CrosshairColor(const Player& player) const;

    // Beam, held clump and flares for the renderer
    SceneVisuals Visuals() const;

    bool& PaintMode() { return m_PaintMode; }
    int& SelectedBlock() { return m_SelectedBlock; } // 1 = Grass, 2 = Dirt, 3 = Stone, 4 = Sand, 5 = Plant
    int ArtifactsRetrieved() const { return m_ArtifactsRetrieved; }
    float ClosestArtifactDistance() const { return m_ClosestArtifactDistance; } // Voxels, -1 = none in range
    bool CarryingArtifact() const { return m_Clump.isArtifact; }

private:
    struct Flare {
        glm::vec3 position;
        glm::vec3 velocity; // World units / second
        glm::vec3 color;
        float intensity;
        bool isStuck;
        float life;
    };

    struct ThrownClump {
        glm::vec3 position;
        glm::vec3 velocity; // World units / second
        float radius;
        bool active;
    };

    void LaunchHeldClump(const Player& player);

    VoxelWorld& m_World;
    WorldEditor& m_Editor;
    glm::ivec2 m_BeaconColumn = glm::ivec2(640, 640);

    std::vector<Flare> m_Flares;
    std::vector<ThrownClump> m_ThrownClumps;
    SceneVisuals::Clump m_Clump; // Held by the tether
    SceneVisuals::Beam m_Beam;

    bool m_PaintMode = false;
    int m_SelectedBlock = 1;
    int m_ArtifactsRetrieved = 0;
    float m_ClosestArtifactDistance = -1.0f;
    float m_ScanTimer = 0.0f;
    int m_NextFlareColor = 0;
    bool m_FlareKeyWasPressed = false;
    bool m_RightWasPressed = false;
};
