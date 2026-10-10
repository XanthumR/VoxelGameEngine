#include "Gameplay/StrategyCamera.h"

#include "World/WorldConstants.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace {

const float ROTATE_SPEED = 90.0f;       // Degrees per second
const float PAN_SPEED = 1.2f;           // Camera distances per second
const float ZOOM_STEP = 0.85f;          // Distance factor per wheel notch
const double EDGE_PAN_MARGIN = 8.0;     // Pixels from the window edge

} // namespace

StrategyCamera::StrategyCamera() {
    UpdatePose();
}

void StrategyCamera::SetTarget(glm::vec3 target) {
    m_Target = target;
    m_PanProgress = 1.0f; // Ends a glide
    UpdatePose();
}

void StrategyCamera::PanTo(glm::vec3 target) {
    m_PanFrom = m_Target;
    m_PanTo = target;
    m_PanProgress = 0.0f;
    // Half a second for a short way, at most two for a long one (in camera distances)
    float distances = glm::length(target - m_Target) / std::max(m_Distance / VOXELS_PER_UNIT, 1e-4f);
    m_PanSeconds = std::clamp(0.4f + distances * 0.25f, 0.5f, 2.0f);
}

void StrategyCamera::SetYaw(float yaw) {
    m_Yaw = yaw;
    UpdatePose();
}

void StrategyCamera::OnScroll(double offset) {
    m_PendingScroll += (float)offset;
}

// The camera sits m_Distance voxels back from the target along the view direction
void StrategyCamera::UpdatePose() {
    float yaw = glm::radians(m_Yaw), pitch = glm::radians(m_Pitch);
    m_Front = glm::normalize(glm::vec3(std::cos(pitch) * std::cos(yaw), -std::sin(pitch), std::cos(pitch) * std::sin(yaw)));
    m_Position = m_Target - m_Front * (m_Distance / VOXELS_PER_UNIT);
}

void StrategyCamera::Update(GLFWwindow* window, float deltaTime, bool mouseFree, bool keyboardFree) {
    // Zoom
    if (m_PendingScroll != 0.0f) {
        m_Distance = std::clamp(m_Distance * std::pow(ZOOM_STEP, m_PendingScroll), MIN_DISTANCE, MAX_DISTANCE);
        m_PendingScroll = 0.0f;
    }

    // Pan directions on the ground, relative to where the camera looks
    glm::vec3 forward = glm::normalize(glm::vec3(m_Front.x, 0.0f, m_Front.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec2 pan(0.0f); // x = right, y = forward

    if (keyboardFree) {
        auto down = [window](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };
        if (down(GLFW_KEY_W) || down(GLFW_KEY_UP)) pan.y += 1.0f;
        if (down(GLFW_KEY_S) || down(GLFW_KEY_DOWN)) pan.y -= 1.0f;
        if (down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT)) pan.x += 1.0f;
        if (down(GLFW_KEY_A) || down(GLFW_KEY_LEFT)) pan.x -= 1.0f;
        if (down(GLFW_KEY_Q)) m_Yaw -= ROTATE_SPEED * deltaTime;
        if (down(GLFW_KEY_E)) m_Yaw += ROTATE_SPEED * deltaTime;
    }

    double cursorX, cursorY;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Screen-edge pan, only while the window has focus and the cursor is inside it
    bool cursorInside = cursorX >= 0.0 && cursorY >= 0.0 && cursorX < width && cursorY < height;
    if (mouseFree && cursorInside && glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        if (cursorX < EDGE_PAN_MARGIN) pan.x -= 1.0f;
        if (cursorX > width - EDGE_PAN_MARGIN) pan.x += 1.0f;
        if (cursorY < EDGE_PAN_MARGIN) pan.y += 1.0f;
        if (cursorY > height - EDGE_PAN_MARGIN) pan.y -= 1.0f;
    }

    float worldDistance = m_Distance / VOXELS_PER_UNIT;
    bool middle = mouseFree && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    if (pan != glm::vec2(0.0f) || middle) m_PanProgress = 1.0f; // Taking over by hand ends a glide
    if (m_PanProgress < 1.0f) {
        m_PanProgress = std::min(1.0f, m_PanProgress + deltaTime / m_PanSeconds);
        float t = m_PanProgress;
        float eased = t * t * (3.0f - 2.0f * t); // Smoothstep: eases in and out
        m_Target = m_PanFrom + (m_PanTo - m_PanFrom) * eased;
    }
    if (pan != glm::vec2(0.0f)) {
        glm::vec2 direction = glm::normalize(pan);
        m_Target += (right * direction.x + forward * direction.y) * (PAN_SPEED * worldDistance * deltaTime);
    }

    // Middle-drag pan: the ground follows the cursor (scaled by zoom)
    if (middle && m_Dragging) {
        float pixelsToWorld = worldDistance * 1.5f / (float)std::max(height, 1);
        m_Target -= right * (float)(cursorX - m_DragX) * pixelsToWorld;
        m_Target += forward * (float)(cursorY - m_DragY) * pixelsToWorld;
    }
    m_Dragging = middle;
    m_DragX = cursorX;
    m_DragY = cursorY;

    m_Pitch = std::clamp(m_Pitch, MIN_PITCH, MAX_PITCH);
    UpdatePose();
}
