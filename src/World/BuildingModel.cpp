#include "World/BuildingModel.h"

#include <cstring>
#include <fstream>
#include <iostream>

bool BuildingModel::Load(const std::string& path) {
    width = depth = height = 0;
    ids.clear();
    smokeEmitters.clear();
    boatBerths.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char magic[4];
    int32_t version = 0;
    file.read(magic, 4);
    file.read((char*)&version, 4);
    if (!file || std::memcmp(magic, "VOX ", 4) != 0) {
        std::cerr << "Not a MagicaVoxel file: " << path << std::endl;
        return false;
    }

    // Chunks: MAIN holds SIZE then XYZI (the first model only); everything else is skipped
    int32_t sizeX = 0, sizeY = 0, sizeZ = 0;
    bool haveVoxels = false;
    while (!haveVoxels) {
        char chunkId[4];
        int32_t contentSize = 0, childrenSize = 0;
        file.read(chunkId, 4);
        file.read((char*)&contentSize, 4);
        file.read((char*)&childrenSize, 4);
        if (!file) break;

        if (std::memcmp(chunkId, "MAIN", 4) == 0) continue; // Its children follow directly
        if (std::memcmp(chunkId, "SIZE", 4) == 0) {
            file.read((char*)&sizeX, 4);
            file.read((char*)&sizeY, 4);
            file.read((char*)&sizeZ, 4);
            if (sizeX <= 0 || sizeY <= 0 || sizeZ <= 0 || sizeX > 256 || sizeY > 256 || sizeZ > 256) return false;
            width = sizeX;
            depth = sizeY;
            height = sizeZ;
            ids.assign((size_t)width * depth * height, 0);
        } else if (std::memcmp(chunkId, "XYZI", 4) == 0) {
            if (ids.empty()) return false; // XYZI before SIZE
            int32_t count = 0;
            file.read((char*)&count, 4);
            for (int32_t i = 0; i < count && file; i++) {
                uint8_t v[4]; // x, y, z, palette index
                file.read((char*)v, 4);
                if (v[0] >= sizeX || v[1] >= sizeY || v[2] >= sizeZ) continue;
                int u = width - 1 - v[0];
                glm::ivec3 position(u, v[2], v[1]);
                if (v[3] == SMOKE_EMITTER) smokeEmitters.push_back({ position.x, position.z, position.y });
                else if (v[3] == BOAT_BERTH) boatBerths.push_back({ position.x, position.z, position.y });
                else ids[(size_t)u + (size_t)width * ((size_t)v[1] + (size_t)depth * v[2])] = v[3];
            }
            haveVoxels = (bool)file;
        } else {
            file.seekg(contentSize + childrenSize, std::ios::cur);
        }
    }

    if (!haveVoxels) {
        width = depth = height = 0;
        ids.clear();
        std::cerr << "No voxels in " << path << std::endl;
        return false;
    }
    return true;
}
