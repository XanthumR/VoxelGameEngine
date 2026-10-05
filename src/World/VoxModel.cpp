#include "World/VoxModel.h"

#include <fstream>
#include <iostream>

bool VoxModel::Load(const std::string& path) {
    voxels.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open .vox file: " << path << std::endl;
        return false;
    }

    // 1. Magic number "VOX " and version
    char magic[4];
    int version;
    file.read(magic, 4);
    file.read((char*)&version, 4);
    if (!file || std::string(magic, 4) != "VOX ") {
        std::cerr << "Not a valid MagicaVoxel file!" << std::endl;
        return false;
    }

    int sizeX = 0, sizeY = 0, sizeZ = 0;

    // 2. Chunks
    while (true) {
        char chunkId[4];
        int contentSize, childrenSize;
        file.read(chunkId, 4);
        file.read((char*)&contentSize, 4);
        file.read((char*)&childrenSize, 4);
        if (!file) break;

        std::string id(chunkId, 4);
        if (id == "MAIN") {
            continue; // Container chunk, its children follow directly
        }
        else if (id == "SIZE") {
            file.read((char*)&sizeX, 4);
            file.read((char*)&sizeY, 4);
            file.read((char*)&sizeZ, 4);
        }
        else if (id == "XYZI") {
            int numVoxels;
            file.read((char*)&numVoxels, 4);

            for (int i = 0; i < numVoxels && file; ++i) {
                unsigned char v[4]; // x, y, z, colorIndex
                file.read((char*)v, 4);

                // The palette index is kept as the block ID so it matches the colors in shade.comp.
                // Slots that collide with gameplay blocks: 1-4 are terrain (grass, dirt, stone,
                // sand), so they move to 36-39 (same colors); 30 and 32 (cavern glow, and a retired gameplay block)
                // are swapped for similar leaf colors.
                int blockType = v[3];
                if (blockType >= 1 && blockType <= 4) blockType += 35;
                else if (blockType == 30) blockType = 31;
                else if (blockType == 32) blockType = 28;

                // MagicaVoxel uses Z as the vertical "Up" axis, so swap Y and Z;
                // its Y is mirrored to keep the model's handedness in a Y-up world
                voxels.push_back({ (int)v[0] - sizeX / 2, (int)v[2], (sizeY - 1 - (int)v[1]) - sizeY / 2, blockType });
            }
        }
        else {
            file.seekg(contentSize, std::ios::cur); // Unhandled chunk (e.g. the RGBA palette)
        }
    }

    if (voxels.empty()) return false;
    min = max = glm::ivec3(voxels[0].x, voxels[0].y, voxels[0].z);
    for (const VoxelOffset& voxel : voxels) {
        min = glm::min(min, glm::ivec3(voxel.x, voxel.y, voxel.z));
        max = glm::max(max, glm::ivec3(voxel.x, voxel.y, voxel.z));
    }
    return true;
}
