#include "Gameplay/PlayerTools.h"

#include "Gameplay/Player.h"
#include "World/BlockTypes.h"
#include "World/Raycast.h"
#include "World/VoxelWorld.h"
#include "World/WorldEditor.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace {

// Gameplay tuning was authored per frame at 165 FPS; this converts it to per second
const float REFERENCE_FPS = 165.0f;

} // namespace

PlayerTools::PlayerTools(VoxelWorld& world, WorldEditor& editor) : m_World(world), m_Editor(editor) {}

void PlayerTools::HandleFlareKey(GLFWwindow* window, const Player& player) {
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!m_FlareKeyWasPressed && m_Flares.size() < SceneVisuals::MAX_FLARES) {
            const glm::vec3 colors[] = {
                glm::vec3(1.0f, 0.4f, 0.1f), // Orange
                glm::vec3(0.1f, 0.8f, 1.0f), // Cyan
                glm::vec3(0.8f, 0.1f, 1.0f)  // Purple
            };
            Flare flare;
            flare.position = player.Position();
            flare.velocity = player.Front() * 0.015f * REFERENCE_FPS;
            flare.color = colors[m_NextFlareColor % 3];
            m_NextFlareColor++;
            flare.intensity = 1.5f;
            flare.isStuck = false;
            flare.life = 30.0f;
            m_Flares.push_back(flare);
            m_FlareKeyWasPressed = true;
        }
    } else {
        m_FlareKeyWasPressed = false;
    }
}

void PlayerTools::HandleBlockSelectKeys(GLFWwindow* window) {
    const int keys[] = { GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5 };
    for (int i = 0; i < 5; i++) {
        if (glfwGetKey(window, keys[i]) == GLFW_PRESS) m_SelectedBlock = i + 1;
    }
}

void PlayerTools::LaunchHeldClump(const Player& player) {
    ThrownClump clump;
    clump.position = m_Clump.position;
    clump.velocity = player.Front() * 0.5f * REFERENCE_FPS;
    clump.radius = m_Clump.radius;
    clump.active = true;
    m_ThrownClumps.push_back(clump);
    m_Clump.active = false;
    m_Clump.isArtifact = false;
    m_Beam.active = false;
}

void PlayerTools::HandleMouse(GLFWwindow* window, float deltaTime, const Player& player) {
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
        m_Beam.active = false;
        return;
    }
    const glm::vec3 position = player.Position(), front = player.Front(), up = player.Up();

    // --- Matter Dissolver (Left Click) ---
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        RayHit hit = Raycast(m_World, position, front, REACH_DISTANCE);
        if (hit.hit) {
            m_Editor.FillSphere(hit.mapPos, 2, Block::AIR);

            // Red beam from the right hand to the hit
            glm::vec3 right = glm::normalize(glm::cross(front, up));
            m_Beam.active = true;
            m_Beam.start = position + front * 0.1f + right * 0.03f + up * -0.02f;
            m_Beam.end = glm::vec3(hit.mapPos) / VOXELS_PER_UNIT;
            m_Beam.color = glm::vec3(1.0f, 0.2f, 0.1f);
        } else {
            m_Beam.active = false;
        }
    } else if (!m_Clump.active) {
        m_Beam.active = false;
    }

    bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    // --- Block Placement (Right Click in Paint Mode) ---
    if (m_PaintMode && !m_Clump.active) {
        if (rightPressed && !m_RightWasPressed) {
            RayHit hit = Raycast(m_World, position, front, REACH_DISTANCE);
            if (hit.hit && hit.normal != glm::ivec3(0)) {
                glm::ivec3 target = hit.mapPos + hit.normal;
                if (!player.OverlapsVoxel(target)) m_Editor.PlaceVoxel(target, (uint8_t)m_SelectedBlock);
            }
        }
        m_RightWasPressed = rightPressed;
        return;
    }

    // --- Gravity Tether (Right Click) ---
    if (rightPressed) {
        if (!m_Clump.active) {
            if (!m_RightWasPressed) {
                RayHit hit = Raycast(m_World, position, front, REACH_DISTANCE);
                if (hit.hit) {
                    uint8_t block = m_World.GetVoxel(hit.mapPos.x, hit.mapPos.y, hit.mapPos.z);
                    if (block > 0) {
                        m_Clump.active = true;
                        m_Clump.isArtifact = (block == Block::ARTIFACT);
                        m_Clump.position = glm::vec3(hit.mapPos) / VOXELS_PER_UNIT;
                        m_Editor.FillSphere(hit.mapPos, 2, Block::AIR);
                    }
                }
            }
        } else {
            // The clump follows a point in front of the camera
            glm::vec3 targetPos = position + front * 0.05f + glm::vec3(0.0f, -0.01f, 0.0f);
            float follow = 1.0f - std::pow(0.9f, deltaTime * REFERENCE_FPS);
            m_Clump.position = glm::mix(m_Clump.position, targetPos, follow);

            // Blue beam from the left hand to the clump
            glm::vec3 right = glm::normalize(glm::cross(front, up));
            m_Beam.active = true;
            m_Beam.start = position + front * 0.1f + right * -0.03f + up * -0.02f;
            m_Beam.end = m_Clump.position;
            m_Beam.color = glm::vec3(0.1f, 0.5f, 1.0f);

            // Artifact delivered to the beacon
            if (m_Clump.isArtifact) {
                glm::vec2 beacon = glm::vec2(m_BeaconColumn) / VOXELS_PER_UNIT;
                float distToBeacon = glm::distance(glm::vec2(m_Clump.position.x, m_Clump.position.z), beacon);
                if (distToBeacon < 0.03f) {
                    m_ArtifactsRetrieved++;
                    m_Clump.active = false;
                    m_Clump.isArtifact = false;
                    m_Beam.active = false;
                }
            }
        }
    } else if (m_Clump.active) {
        LaunchHeldClump(player);
    }
    m_RightWasPressed = rightPressed;
}

void PlayerTools::UpdateProjectiles(float deltaTime) {
    for (Flare& flare : m_Flares) {
        if (flare.life > 0.0f) flare.life -= deltaTime;
        if (flare.life < 2.0f) flare.intensity = (flare.life / 2.0f) * 1.2f; // Fade out

        if (!flare.isStuck) {
            flare.velocity.y -= 0.04f * REFERENCE_FPS * deltaTime; // Gravity
            glm::vec3 step = flare.velocity * deltaTime;
            float length = glm::length(step);
            if (length > 0.0f) {
                // Swept test so fast flares cannot pass through thin walls
                RayHit hit = Raycast(m_World, flare.position, step / length, length);
                if (hit.hit) {
                    // Rest on the face that was hit so the light is not inside the block
                    flare.position = (glm::vec3(hit.mapPos) + 0.5f + glm::vec3(hit.normal) * 0.6f) / VOXELS_PER_UNIT;
                    flare.isStuck = true;
                    flare.velocity = glm::vec3(0.0f);
                } else {
                    flare.position += step;
                }
            }
        }
    }
    m_Flares.erase(std::remove_if(m_Flares.begin(), m_Flares.end(), [](const Flare& f) { return f.life <= 0.0f; }), m_Flares.end());

    for (ThrownClump& clump : m_ThrownClumps) {
        if (!clump.active) continue;

        clump.velocity.y -= 0.08f * REFERENCE_FPS * deltaTime; // Gravity
        glm::vec3 step = clump.velocity * deltaTime;
        float length = glm::length(step);
        if (length > 0.0f) {
            RayHit hit = Raycast(m_World, clump.position, step / length, length);
            if (hit.hit) {
                m_Editor.FillSphere(hit.mapPos, 4, Block::AIR); // Explode
                clump.active = false;
                continue;
            }
            clump.position += step;
        }

        float voxelY = clump.position.y * VOXELS_PER_UNIT;
        if (voxelY < 0.0f || voxelY >= (float)WORLD_HEIGHT) clump.active = false;
    }
    m_ThrownClumps.erase(std::remove_if(m_ThrownClumps.begin(), m_ThrownClumps.end(),
        [](const ThrownClump& c) { return !c.active; }), m_ThrownClumps.end());
}

void PlayerTools::UpdateScanner(float deltaTime, const Player& player) {
    m_ScanTimer += deltaTime;
    if (m_ScanTimer < 0.2f) return;
    m_ScanTimer = 0.0f;

    const glm::vec3 position = player.Position();
    float minDist = 1e9f;
    m_World.ForEachChunk([&](Chunk& chunk) {
        if (chunk.artifactIdx.empty()) return;

        glm::ivec3 base = chunk.position * CHUNK_SIZE;
        glm::vec3 chunkCenter = glm::vec3(base + glm::ivec3(16)) / VOXELS_PER_UNIT;
        if (glm::distance(position, chunkCenter) > 0.3f) return;

        // Drop entries whose artifact was dug out or picked up
        std::vector<uint16_t>& list = chunk.artifactIdx;
        list.erase(std::remove_if(list.begin(), list.end(), [&chunk](uint16_t i) {
            return chunk.data[i] != Block::ARTIFACT;
        }), list.end());

        for (uint16_t i : list) {
            glm::ivec3 local(i % 32, (i / 32) % 32, i / 1024);
            glm::vec3 artifactPos = glm::vec3(base + local) / VOXELS_PER_UNIT;
            minDist = std::min(minDist, glm::distance(position, artifactPos));
        }
    });
    m_ClosestArtifactDistance = minDist < 1e8f ? minDist * VOXELS_PER_UNIT : -1.0f;
}

glm::vec3 PlayerTools::CrosshairColor(const Player& player) const {
    RayHit hit = Raycast(m_World, player.Position(), player.Front(), REACH_DISTANCE);
    if (hit.hit) {
        uint8_t block = m_World.GetVoxel(hit.mapPos.x, hit.mapPos.y, hit.mapPos.z);
        if (block == Block::ARTIFACT) return glm::vec3(1.0f, 0.0f, 1.0f);
        if (block > 0) return glm::vec3(0.1f, 1.0f, 0.3f);
    }
    return glm::vec3(1.0f);
}

SceneVisuals PlayerTools::Visuals() const {
    SceneVisuals visuals;
    visuals.beam = m_Beam;
    visuals.clump = m_Clump;
    visuals.flareCount = (int)std::min<size_t>(m_Flares.size(), SceneVisuals::MAX_FLARES);
    for (int i = 0; i < visuals.flareCount; i++) {
        visuals.flares[i] = { m_Flares[i].position, m_Flares[i].color, m_Flares[i].intensity };
    }
    return visuals;
}
