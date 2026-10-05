#include "Core/LaunchOptions.h"

#include "Rendering/RenderSettings.h"
#include "World/ChunkStreamer.h"

#include <algorithm>
#include <cstdlib>

LaunchOptions LaunchOptions::Parse(int argc, char** argv) {
    LaunchOptions options;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        int remaining = argc - 1 - i;
        if (arg == "--render-distance" && remaining >= 1) {
            options.renderDistance = std::clamp(std::atoi(argv[++i]), ChunkStreamer::MIN_RENDER_DISTANCE, ChunkStreamer::MAX_RENDER_DISTANCE);
        } else if (arg == "--render-scale" && remaining >= 1) {
            options.renderScale = std::clamp((float)std::atof(argv[++i]), RenderSettings::MIN_RENDER_SCALE, 1.0f);
        } else if (arg == "--fixed-time" && remaining >= 1) {
            options.fixedTime = (float)std::atof(argv[++i]);
        } else if (arg == "--camera" && remaining >= 5) {
            options.lockCamera = true;
            options.cameraVoxel = glm::vec3((float)std::atof(argv[i + 1]), (float)std::atof(argv[i + 2]), (float)std::atof(argv[i + 3]));
            options.cameraYaw = (float)std::atof(argv[i + 4]);
            options.cameraPitch = (float)std::atof(argv[i + 5]);
            i += 5;
        } else if (arg == "--screenshot" && remaining >= 2) {
            options.screenshotPath = argv[++i];
            options.screenshotDelay = std::atof(argv[++i]);
        }
    }
    return options;
}
