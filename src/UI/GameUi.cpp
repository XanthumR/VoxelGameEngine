#include "UI/GameUi.h"

#include "RmlUi_Platform_GLFW.h"
#include "RmlUi_Renderer_GL3.h"

#include <GLFW/glfw3.h>
#include <RmlUi/Core.h>

#include <iostream>

namespace {

// Variable fonts: RmlUi loads each of their weights (Cinzel for titles, Libre Baskerville for text)
const char* FONTS[] = { "assets/ui/fonts/LibreBaskerville.ttf", "assets/ui/fonts/Cinzel.ttf" };

// The GLFW system interface, with RmlUi's warnings and errors (a broken document) on the console
class LoggingSystemInterface : public SystemInterface_GLFW {
public:
    using SystemInterface_GLFW::SystemInterface_GLFW;
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        if (type <= Rml::Log::LT_WARNING) std::cerr << "RmlUi: " << message << std::endl;
        return true;
    }
};

} // namespace

GameUi::GameUi() = default;

GameUi::~GameUi() {
    Shutdown();
}

bool GameUi::Init(GLFWwindow* window) {
    m_Window = window;
    m_System = std::make_unique<LoggingSystemInterface>(window);
    m_Renderer = std::make_unique<RenderInterface_GL3>();
    if (!*m_Renderer) {
        std::cerr << "RmlUi: the OpenGL 3 renderer failed to start" << std::endl;
        return false;
    }
    Rml::SetSystemInterface(m_System.get());
    Rml::SetRenderInterface(m_Renderer.get());
    if (!Rml::Initialise()) return false;
    m_Initialised = true;
    for (const char* font : FONTS) {
        if (!Rml::LoadFontFace(font)) return false;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    m_Context = Rml::CreateContext("game", Rml::Vector2i(width, height));
    return m_Context != nullptr;
}

void GameUi::Shutdown() {
    if (m_Initialised) {
        Rml::Shutdown(); // Also destroys the context and its documents
        m_Initialised = false;
        m_Context = nullptr;
    }
    m_Renderer.reset(); // Its GL objects go while the context is still current
    m_System.reset();
}

void GameUi::Update(int width, int height) {
    if (!m_Context) return;
    if (m_Context->GetDimensions() != Rml::Vector2i(width, height)) m_Context->SetDimensions(Rml::Vector2i(width, height));
    m_Context->Update();
}

void GameUi::Render(int width, int height) {
    if (!m_Context) return;
    m_Renderer->SetViewport(width, height);
    m_Renderer->BeginFrame();
    m_Context->Render();
    m_Renderer->EndFrame();
}

bool GameUi::WantsMouse() const {
    return m_Context && m_Context->IsMouseInteracting();
}

bool GameUi::OnKey(int key, int action, int mods) {
    m_Mods = mods;
    return m_Context && !RmlGLFW::ProcessKeyCallback(m_Context, key, action, mods);
}

bool GameUi::OnChar(unsigned int codepoint) {
    return m_Context && !RmlGLFW::ProcessCharCallback(m_Context, codepoint);
}

bool GameUi::OnCursorPos(double x, double y) {
    return m_Context && !RmlGLFW::ProcessCursorPosCallback(m_Context, m_Window, x, y, m_Mods);
}

bool GameUi::OnMouseButton(int button, int action, int mods) {
    m_Mods = mods;
    return m_Context && !RmlGLFW::ProcessMouseButtonCallback(m_Context, button, action, mods);
}

bool GameUi::OnScroll(double yOffset) {
    return m_Context && !RmlGLFW::ProcessScrollCallback(m_Context, yOffset, m_Mods);
}
