#include <vector>
#include <glm/glm.hpp>

// 32-byte aligned struct for OpenGL std430
struct AnimatedPlant {
    glm::ivec4 posAndFrame;   // x, y, z, currentFrame
    glm::vec4 timeData;// randomTimeOffset, padding, padding, padding
};

std::vector<AnimatedPlant> activePlants;

struct AnimFrame {
    std::vector<glm::ivec3> voxelOffsets;
};
