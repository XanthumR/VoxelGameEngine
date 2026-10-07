#pragma once

#include <memory>

struct GLFWwindow;
class RenderInterface_GL3;
class SystemInterface_GLFW;
namespace Rml {
class Context;
}

// The player-facing UI, made with RmlUi: documents and style sheets from assets/ui, drawn over the
// scene before ImGui's debug windows. The window's GLFW callbacks pass input on to it first; the
// game only takes the mouse where WantsMouse is false.
class GameUi {
public:
    GameUi();
    ~GameUi();

    // After the OpenGL context exists; false when RmlUi or its fonts fail to load
    bool Init(GLFWwindow* window);
    void Shutdown();

    // Once per frame, with the framebuffer size
    void Update(int width, int height);
    void Render(int width, int height);

    // The cursor is over a UI element (or dragging one)
    bool WantsMouse() const;
    Rml::Context* Context() { return m_Context; }

    // From the GLFW callbacks; each returns true when the UI used the event
    bool OnKey(int key, int action, int mods);
    bool OnChar(unsigned int codepoint);
    bool OnCursorPos(double x, double y);
    bool OnMouseButton(int button, int action, int mods);
    bool OnScroll(double yOffset);

private:
    GLFWwindow* m_Window = nullptr;
    std::unique_ptr<SystemInterface_GLFW> m_System;
    std::unique_ptr<RenderInterface_GL3> m_Renderer;
    Rml::Context* m_Context = nullptr;
    bool m_Initialised = false;
    int m_Mods = 0; // Key modifiers from the last key or button event (GLFW gives none with the cursor)
};
