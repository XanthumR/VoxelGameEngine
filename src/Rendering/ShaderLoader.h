#pragma once

#include <glad/glad.h>

#include <string>

// Root folder of the shader files, relative to the working directory
constexpr const char* SHADER_ROOT = "shaders/";

// Loads, compiles and links a compute shader from SHADER_ROOT + path. Lines of the form
// #include "file" are replaced by that file (also relative to SHADER_ROOT); each file is
// included at most once per program, so includes may include their own dependencies.
// Returns 0 on failure, after printing the error.
GLuint LoadComputeProgram(const std::string& path);
