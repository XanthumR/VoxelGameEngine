#pragma once

#include <string>

// Saves the window's framebuffer as a binary PPM (top row first). Returns false on failure.
bool SaveWindowScreenshot(const std::string& path, int width, int height);
