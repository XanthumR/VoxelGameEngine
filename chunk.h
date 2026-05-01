#include <glad/glad.h>
#include <GLFW/glfw3.h>
// --- Helper Math Includes (GLM) ---
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <stdio.h>
#include <vector>
#include <unordered_map>

struct Chunk {
    glm::ivec3 position; // e.g., (0, 0, 0), (1, 0, 0)...
    std::vector<GLubyte> data;
    GLuint textureID; // Each chunk gets its own small 3D texture
    bool isModified = false;
};

// Your "World"
std::unordered_map<uint64_t, Chunk*> worldMap;