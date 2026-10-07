// RmlUi's own OpenGL 3 renderer and GLFW platform backends, compiled from the RmlUi folder
// (RmlUiDir) as one unit, on the game's OpenGL loader and without the project's warning level.
#define RMLUI_GL3_CUSTOM_LOADER <glad/glad.h>

#pragma warning(push, 0)
#include "RmlUi_Renderer_GL3.cpp"
#include "RmlUi_Platform_GLFW.cpp"
#pragma warning(pop)
