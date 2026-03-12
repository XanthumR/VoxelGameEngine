#define _CRT_SECURE_NO_WARNINGS
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <cmath>
#include "FastNoiseLite.h"
#include "rayCast.h"

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


char* shaderLoader(const char* file) {
    FILE* fp = fopen(file, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open shader file: %s\n", file);
        return NULL;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    // Allocate buffer (+1 for null terminator)
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for shader\n");
        fclose(fp);
        return NULL;
    }

    // Read file into buffer
    fread(buffer, 1, size, fp);
    buffer[size] = '\0';

    fclose(fp);
    return buffer;
}

void checkMouseInput(GLFWwindow* window, std::vector<GLubyte>& voxelData, GLuint voxelTex){

    // Inside main loop:
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        RayHit hit = raycast(cameraPos, cameraFront, 1.1f,VOXEL_WIDTH,VOXEL_HEIGHT,VOXEL_DEPTH,voxelData); // 0.1 is reach distance in world units
        if (hit.hit) {
            size_t idx = ((size_t)hit.mapPos.z * VOXEL_WIDTH * VOXEL_HEIGHT) + ((size_t)hit.mapPos.y * VOXEL_WIDTH) + hit.mapPos.x;
            voxelData[idx] = 0; // Remove block in CPU data

            // Update GPU texture (1x1x1 pixel)
            GLubyte val = 0;
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, voxelTex);
            glTexSubImage3D(GL_TEXTURE_3D, 0, hit.mapPos.x, hit.mapPos.y, hit.mapPos.z, 1, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &val);
        }
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        RayHit hit = raycast(cameraPos, cameraFront, 1.1f, VOXEL_WIDTH, VOXEL_HEIGHT, VOXEL_DEPTH, voxelData);
        if (hit.hit) {
            glm::ivec3 placePos = hit.mapPos + hit.normal; // Offset by normal to place NEXT to the block

            // Bounds check
            if (placePos.x >= 0 && placePos.x < VOXEL_WIDTH) {
                size_t idx = ((size_t)placePos.z * VOXEL_WIDTH * VOXEL_HEIGHT) + ((size_t)placePos.y * VOXEL_WIDTH) + placePos.x;
                voxelData[idx] = 3; // Place Stone

                GLubyte val = 3;
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_3D, voxelTex);
                glTexSubImage3D(GL_TEXTURE_3D, 0, placePos.x, placePos.y, placePos.z, 1, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &val);
            }
        }
    }
}

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
char* computeShaderSource = shaderLoader("default.comp");

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
    glfwSwapInterval(1);
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
    free(computeShaderSource);
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
    bool leftMouseWasPressed = false;
    bool rightMouseWasPressed = false;
    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // Process Input
        processInput(window, deltaTime);   
        bool leftMouseIsPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rightMouseIsPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        if (leftMouseIsPressed && !leftMouseWasPressed) {
            checkMouseInput(window, voxelData, voxelTex);
        }
        if (rightMouseIsPressed && !rightMouseWasPressed)
        {
            checkMouseInput(window, voxelData, voxelTex);
        }
        leftMouseWasPressed = leftMouseIsPressed;
        rightMouseWasPressed = rightMouseIsPressed;
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