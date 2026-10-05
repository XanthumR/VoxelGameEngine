#pragma once

#include <glm/glm.hpp>

// A camera the renderer can draw from. All positions are in world units.
class ICamera {
public:
    virtual ~ICamera() = default;

    virtual glm::vec3 Position() const = 0;
    virtual glm::vec3 Front() const = 0;
    virtual glm::vec3 Up() const = 0;

    // The point the world streams around (the ground point a strategy camera looks at, or the
    // free-fly camera itself)
    virtual glm::vec3 FocusPoint() const = 0;
};
