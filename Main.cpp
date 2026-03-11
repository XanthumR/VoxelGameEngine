#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "FastNoiseLite.h"

// --- Helper Math Includes (GLM) ---
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- Visual Studio Linker Config ---
#ifdef _MSC_VER
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "opengl32.lib")
#endif

// --- Configuration ---
const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;
const int VOXEL_WIDTH = 1280;
const int VOXEL_HEIGHT = 128;
const int VOXEL_DEPTH = 1280;


// --- Camera State (Globals for Callback Access) ---
glm::vec3 cameraPos = glm::vec3(0.5f, 0.5f, 2.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = WINDOW_WIDTH / 2.0f;
float lastY = WINDOW_HEIGHT / 2.0f;

// --- Callbacks & Input ---

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Constrain pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void processInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = (0.5f / ((float)VOXEL_WIDTH / 128.0)) * deltaTime; // Adjust speed as needed
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraSpeed *= 2.5f; // Sprint

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
}

// --- Shader Sources ---

const char* computeShaderSource = R"(
#version 440 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba32f, binding = 0) uniform image2D imgOutput;
layout(binding = 1) uniform sampler3D voxelTex;

uniform vec3 cameraPos;
uniform mat4 inverseView;
uniform mat4 inverseProj;

uniform int dimX;
uniform int dimY;
uniform int dimZ;

struct Ray {
    vec3 origin;
    vec3 dir;
};

vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - rayOrigin) / rayDir;
    vec3 tMax = (boxMax - rayOrigin) / rayDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return vec2(tNear, tFar);
}

Ray createRay(ivec2 pixel_coords, ivec2 dims) {
    vec2 pxNDC = vec2(pixel_coords) / vec2(dims) * 2.0 - 1.0;
    vec4 target = inverseProj * vec4(pxNDC, 1.0, 1.0);
    vec3 rayDirView = normalize(target.xyz / target.w);
    vec3 rayDirWorld = (inverseView * vec4(rayDirView, 0.0)).xyz;
    return Ray(cameraPos, normalize(rayDirWorld));
}

void main() {
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dims = imageSize(imgOutput);
    if (pixel_coords.x >= dims.x || pixel_coords.y >= dims.y) return;

    Ray ray = createRay(pixel_coords, dims);
    vec4 color = vec4(0.5, 0.8, 1.0, 1.0); // Sky Color

    // Scale the AABB to match our world proportions
    vec3 boxMax = vec3(float(dimX)/float(dimX), float(dimY)/float(dimX), float(dimZ)/float(dimX));
    vec2 tBox = intersectAABB(ray.origin, ray.dir, vec3(0.0), boxMax);

    if (tBox.x > tBox.y || tBox.y < 0.0) {
        imageStore(imgOutput, pixel_coords, color);
        return;
    }

    float tStart = max(tBox.x, 0.0);
    vec3 startPos = ray.origin + ray.dir * (tStart + 0.0001);
    
    // Scale position to integer grid space
    vec3 rayPos = startPos * float(dimX); 
    ivec3 mapPos = clamp(ivec3(floor(rayPos)), ivec3(0), ivec3(dimX-1, dimY-1, dimZ-1));

    vec3 deltaDist = abs(1.0 / ray.dir);
    ivec3 stepDir = ivec3(sign(ray.dir));
    vec3 sideDist = (sign(ray.dir) * (vec3(mapPos) - rayPos) + (sign(ray.dir) * 0.5) + 0.5) * deltaDist;

    vec3 normal = vec3(0.0);
    int maxSteps = (dimX + dimY + dimZ) / 2; 

    for (int i = 0; i < maxSteps; i++) {
        // CORRECTED: Use vec3 of dims for normalized texture sampling
        vec3 texCoord = (vec3(mapPos) + 0.5) / vec3(dimX, dimY, dimZ);
        float voxelValue = texture(voxelTex, texCoord).r;

        if (voxelValue > 0.0) {
            // Convert normalized float back to 0-255 integer range
            int voxelID = int(voxelValue * 255.0 + 0.5); 
            
            vec3 albedo = vec3(1.0, 0.0, 1.0); // Error Pink (if ID is unknown)

            if (voxelID == 1) albedo = vec3(0.2, 0.8, 0.1); // Grass
            if (voxelID == 2) albedo = vec3(0.4, 0.2, 0.1); // Dirt
            if (voxelID == 3) albedo = vec3(0.5, 0.5, 0.5); // Stone
            if (voxelID == 4) albedo = vec3(0.9, 0.8, 0.4); // Sand

            // Shading
            vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
            float diff = max(dot(normal, lightDir), 0.3);
            
            // Simple Ambient Occlusion hack (darken bottom blocks)
            float ao = mix(0.7, 1.0, float(mapPos.y) / float(dimY));
            
            color = vec4(albedo * diff * ao, 1.0);
            break;
        }

        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) {
                sideDist.x += deltaDist.x; mapPos.x += stepDir.x;
                normal = vec3(-stepDir.x, 0.0, 0.0);
            } else {
                sideDist.z += deltaDist.z; mapPos.z += stepDir.z;
                normal = vec3(0.0, 0.0, -stepDir.z);
            }
        } else {
            if (sideDist.y < sideDist.z) {
                sideDist.y += deltaDist.y; mapPos.y += stepDir.y;
                normal = vec3(0.0, -stepDir.y, 0.0);
            } else {
                sideDist.z += deltaDist.z; mapPos.z += stepDir.z;
                normal = vec3(0.0, 0.0, -stepDir.z);
            }
        }

        if (mapPos.x < 0 || mapPos.x >= dimX || mapPos.y < 0 || mapPos.y >= dimY || mapPos.z < 0 || mapPos.z >= dimZ) break;
    }

    imageStore(imgOutput, pixel_coords, color);
}
)";

void checkShaderErrors(GLuint shader, std::string type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
        }
    }
}

int main() {
    // 1. Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL Compute DDA Voxel", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // --- Input Setup ---
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 2. Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // 3. Create Output Texture (Screen buffer)
    GLuint screenTex;
    glGenTextures(1, &screenTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screenTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // 4. Create and Populate Voxel 3D Texture
    size_t vSize = (size_t)VOXEL_WIDTH * VOXEL_HEIGHT * VOXEL_DEPTH;
    std::vector<GLubyte> voxelData(vSize, 0);

// 1. Initialize Noise (Example using FastNoiseLite)
// FastNoiseLite noise;
// noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

    // Define a fixed scale (higher number = jagged features)
    float noiseScale = 0.5f;

    for (int z = 0; z < VOXEL_DEPTH; z++) {
        for (int x = 0; x < VOXEL_WIDTH; x++) {

            float heightSample = noise.GetNoise((float)x * noiseScale, (float)z * noiseScale);
            int terrainHeight = static_cast<int>((heightSample + 1.0f) * 0.5f * (VOXEL_HEIGHT - 10)) + 5;

            for (int y = 0; y < VOXEL_HEIGHT; y++) {
                // THE FIX: Correct 1D Indexing for Width x Height x Depth
                size_t idx = ((size_t)z * VOXEL_WIDTH * VOXEL_HEIGHT) + ((size_t)y * VOXEL_WIDTH) + x;

                if (y < terrainHeight) {
                    if (y < 15) {
                        voxelData[idx] = 4; // Sand (low areas)
                    }
                    else if (y == terrainHeight - 1) {
                        voxelData[idx] = 1; // Grass (top layer)
                    }
                    else if (y > terrainHeight - 4) {
                        voxelData[idx] = 2; // Dirt (just below grass)
                    }
                    else {
                        voxelData[idx] = 3; // Stone (deep)
                    }
                }
                else {
                    voxelData[idx] = 0; // Air
                }
            }
        }
    }

    GLuint voxelTex;
    glGenTextures(1, &voxelTex);
    glActiveTexture(GL_TEXTURE1); // Bind to unit 1
    glBindTexture(GL_TEXTURE_3D, voxelTex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // Nearest for hard voxel edges
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, VOXEL_WIDTH, VOXEL_HEIGHT, VOXEL_DEPTH, 0, GL_RED, GL_UNSIGNED_BYTE, voxelData.data());

    // 5. Setup Compute Shader
    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &computeShaderSource, NULL);
    glCompileShader(computeShader);
    checkShaderErrors(computeShader, "COMPUTE");

    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);
    checkShaderErrors(computeProgram, "PROGRAM");

    // 6. Loop
    float lastTime = 0.0f;

    // FBO for blitting
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTex, 0);

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // Process Input
        processInput(window, deltaTime);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 100.0f);

        // Compute Pass
        glUseProgram(computeProgram);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, voxelTex);
        glUniform1i(glGetUniformLocation(computeProgram, "voxelTex"), 1); // Texture Unit 1
        glUniform1i(glGetUniformLocation(computeProgram, "dimX"), VOXEL_WIDTH);
        glUniform1i(glGetUniformLocation(computeProgram, "dimY"), VOXEL_HEIGHT);
        glUniform1i(glGetUniformLocation(computeProgram, "dimZ"), VOXEL_DEPTH);
        glUniform3fv(glGetUniformLocation(computeProgram, "cameraPos"), 1, &cameraPos[0]);
        glUniformMatrix4fv(glGetUniformLocation(computeProgram, "inverseView"), 1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
        glUniformMatrix4fv(glGetUniformLocation(computeProgram, "inverseProj"), 1, GL_FALSE, glm::value_ptr(glm::inverse(proj)));

        // Dispatch
        glDispatchCompute((WINDOW_WIDTH + 7) / 8, (WINDOW_HEIGHT + 7) / 8, 1);

        // Memory barrier
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Render Pass: Blit
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Screen
        glBlitFramebuffer(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}