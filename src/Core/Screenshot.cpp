#include "Core/Screenshot.h"

#include <glad/glad.h>

#include <cstdio>
#include <vector>

bool SaveWindowScreenshot(const std::string& path, int width, int height) {
    std::vector<unsigned char> pixels((size_t)width * height * 3);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; y--) fwrite(&pixels[(size_t)y * width * 3], 1, (size_t)width * 3, file); // GL rows are bottom-up
    fclose(file);
    return true;
}
