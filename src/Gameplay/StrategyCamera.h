#pragma once

#include "Gameplay/Camera.h"

#include <glm/glm.hpp>

struct GLFWwindow;

// Anno-style city-builder camera: orbits a target point on the ground.
//   WASD / arrow keys / screen edge  pan          Q / E          rotate
//   Mouse wheel                      zoom         Middle drag    pan
// The cursor stays visible for picking and building.
class StrategyCamera : public ICamera {
public:
    static constexpr float MIN_DISTANCE = 25.0f;  // Voxels from the target
    static constexpr float MAX_DISTANCE = 450.0f;
    static constexpr float MIN_PITCH = 35.0f;     // Degrees below the horizon
    static constexpr float MAX_PITCH = 80.0f;

    StrategyCamera();

    // Looks at this point (world units); keeps the current yaw, pitch and zoom
    void SetTarget(glm::vec3 target);
    void SetYaw(float yaw);

    // mouseFree: the UI is not using the mouse, so edge pan, drag and zoom may react
    void Update(GLFWwindow* window, float deltaTime, bool mouseFree, bool keyboardFree);
    void OnScroll(double offset); // Mouse wheel, applied on the next Update

    glm::vec3 Position() const override { return m_Position; }
    glm::vec3 Front() const override { return m_Front; }
    glm::vec3 Up() const override { return glm::vec3(0.0f, 1.0f, 0.0f); }
    glm::vec3 FocusPoint() const override { return m_Target; }

    float Yaw() const { return m_Yaw; }
    float Pitch() const { return m_Pitch; }

private:
    void UpdatePose();

    glm::vec3 m_Target = glm::vec3(0.5f, 0.0f, 0.5f); // World units, on the ground
    float m_Yaw = -90.0f;     // Degrees; -90 looks toward -Z, like the free-fly camera at start
    float m_Pitch = 55.0f;    // Degrees below the horizon
    float m_Distance = 140.0f; // Voxels
    float m_PendingScroll = 0.0f;

    bool m_Dragging = false;
    double m_DragX = 0.0, m_DragY = 0.0;

    glm::vec3 m_Position = glm::vec3(0.0f);
    glm::vec3 m_Front = glm::vec3(0.0f, -1.0f, 0.0f);
};
