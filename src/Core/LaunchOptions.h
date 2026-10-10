#pragma once

#include <glm/glm.hpp>

#include <string>

// Command-line options:
//   --render-distance N        starting render distance in chunks
//   --render-scale F           fraction of the window resolution to trace (0.5 - 1.0)
//   --fixed-time T             freeze time of day, waves and grass at T seconds
//   --camera X Y Z YAW PITCH   hold the camera at this voxel position and direction
//   --screenshot FILE SECONDS  save the window as a PPM after SECONDS, then quit
//   --no-vsync                 draw as fast as possible (for measuring: with vsync the GPU idles,
//                              clocks down and its per-pass times in F3 stretch)
// The last three make runs reproducible, for before/after screenshot comparisons.
struct LaunchOptions {
    int renderDistance = 24;
    float renderScale = 1.0f;
    float fixedTime = -1.0f; // < 0: real time
    bool lockCamera = false;
    glm::vec3 cameraVoxel = glm::vec3(0.0f);
    float cameraYaw = -90.0f;
    float cameraPitch = 0.0f;
    std::string screenshotPath;
    double screenshotDelay = -1.0; // < 0: no screenshot
    bool vsync = true;

    static LaunchOptions Parse(int argc, char** argv);
};
