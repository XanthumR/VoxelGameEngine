// Entry point. The game is assembled in Core/Application; see CLAUDE.md for the layout.
#include "Core/Application.h"
#include "Core/LaunchOptions.h"

#include <cstdlib>

#ifdef _WIN32
// Laptops with two GPUs: the NVIDIA and AMD drivers run a program that exports these on the
// dedicated GPU instead of the integrated one
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#else
#include <filesystem>
#endif

int main(int argc, char** argv) {
#ifndef _WIN32
    // Linux laptops with an NVIDIA GPU run programs on the integrated one unless asked (PRIME
    // render offload, what prime-run does). Only with the NVIDIA driver loaded, and variables
    // already set win: __GLX_VENDOR_LIBRARY_NAME=mesa keeps the integrated GPU.
    if (std::filesystem::exists("/proc/driver/nvidia/version")) {
        setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
        setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
    }
#endif
    Application application(LaunchOptions::Parse(argc, argv));
    return application.Run();
}
