#include "Gameplay/DebugEditTool.h"

#include "Gameplay/Player.h"
#include "World/BlockTypes.h"
#include "World/Raycast.h"
#include "World/VoxelWorld.h"
#include "World/WorldEditor.h"

#include <GLFW/glfw3.h>

DebugEditTool::DebugEditTool(VoxelWorld& world, WorldEditor& editor) : m_World(world), m_Editor(editor) {}

void DebugEditTool::HandleBlockSelectKeys(GLFWwindow* window) {
    const int keys[] = { GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5 };
    for (int i = 0; i < 5; i++) {
        if (glfwGetKey(window, keys[i]) == GLFW_PRESS) m_SelectedBlock = i + 1;
    }
}

void DebugEditTool::HandleMouse(GLFWwindow* window, const Player& player) {
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return;

    // Dig (held: keeps digging every frame)
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        RayHit hit = Raycast(m_World, player.Position(), player.Front(), REACH_DISTANCE);
        if (hit.hit) m_Editor.FillSphere(hit.mapPos, DIG_RADIUS, Block::AIR);
    }

    // Place (one block per click), against the face that was hit
    bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (rightPressed && !m_RightWasPressed) {
        RayHit hit = Raycast(m_World, player.Position(), player.Front(), REACH_DISTANCE);
        if (hit.hit && hit.normal != glm::ivec3(0)) {
            glm::ivec3 target = hit.mapPos + hit.normal;
            if (!player.OverlapsVoxel(target)) m_Editor.PlaceVoxel(target, (uint8_t)m_SelectedBlock);
        }
    }
    m_RightWasPressed = rightPressed;
}

glm::vec3 DebugEditTool::CrosshairColor(const Player& player) const {
    RayHit hit = Raycast(m_World, player.Position(), player.Front(), REACH_DISTANCE);
    return hit.hit ? glm::vec3(0.1f, 1.0f, 0.3f) : glm::vec3(1.0f);
}
