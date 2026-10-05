#include "Rendering/ShaderLoader.h"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace {

bool ReadFile(const std::string& path, std::string& contents) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::stringstream stream;
    stream << file.rdbuf();
    contents = stream.str();
    return true;
}

// Replaces #include "file" lines with the file's contents, recursively, each file once
bool ExpandIncludes(const std::string& path, std::set<std::string>& included, std::string& output) {
    if (!included.insert(path).second) return true; // Already included

    std::string source;
    if (!ReadFile(SHADER_ROOT + path, source)) {
        std::cerr << "Failed to open shader file: " << SHADER_ROOT << path << std::endl;
        return false;
    }

    std::istringstream lines(source);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos && line.compare(start, 8, "#include") == 0) {
            size_t open = line.find('"', start);
            size_t close = line.find('"', open + 1);
            if (open == std::string::npos || close == std::string::npos) {
                std::cerr << "Malformed #include in " << path << ": " << line << std::endl;
                return false;
            }
            if (!ExpandIncludes(line.substr(open + 1, close - open - 1), included, output)) return false;
            continue;
        }
        output += line;
        output += '\n';
    }
    return true;
}

bool CheckStatus(GLuint object, bool isProgram, const std::string& name) {
    GLint success;
    GLchar infoLog[1024];
    if (isProgram) {
        glGetProgramiv(object, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(object, 1024, NULL, infoLog);
            std::cout << "PROGRAM_LINKING_ERROR (" << name << ")\n" << infoLog << "\n";
        }
    } else {
        glGetShaderiv(object, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(object, 1024, NULL, infoLog);
            std::cout << "SHADER_COMPILATION_ERROR (" << name << ")\n" << infoLog << "\n";
        }
    }
    return success != 0;
}

} // namespace

GLuint LoadComputeProgram(const std::string& path) {
    std::set<std::string> included;
    std::string source;
    if (!ExpandIncludes(path, included, source)) return 0;
    const char* sourcePointer = source.c_str();

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &sourcePointer, NULL);
    glCompileShader(shader);

    GLuint program = 0;
    if (CheckStatus(shader, false, path)) {
        program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);
        if (!CheckStatus(program, true, path)) {
            glDeleteProgram(program);
            program = 0;
        }
    }
    glDeleteShader(shader);
    return program;
}
