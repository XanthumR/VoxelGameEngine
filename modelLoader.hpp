#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

struct Offset {
    int x, y, z, blockType;
};

// Loads a MagicaVoxel model as offsets from its root: centered on X/Z, base at y = 0.
// The palette index is kept as the block ID so it matches the colors in default.comp.
inline std::vector<Offset> loadVoxToOffsets(const std::string& filepath) {
    std::vector<Offset> offsets;
    std::ifstream file(filepath, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open .vox file: " << filepath << std::endl;
        return offsets;
    }

    // 1. Read magic number "VOX " and version
    char magic[4];
    int version;
    file.read(magic, 4);
    file.read((char*)&version, 4);

    if (!file || std::string(magic, 4) != "VOX ") {
        std::cerr << "Not a valid MagicaVoxel file!" << std::endl;
        return offsets;
    }

    int sizeX = 0, sizeY = 0, sizeZ = 0;

    // 2. Parse chunks
    while (true) {
        char chunkId[4];
        int contentSize, childrenSize;

        file.read(chunkId, 4);
        file.read((char*)&contentSize, 4);
        file.read((char*)&childrenSize, 4);
        if (!file) break;

        std::string id(chunkId, 4);

        if (id == "MAIN") {
            // Container chunk, its children follow directly
            continue;
        }
        else if (id == "SIZE") {
            file.read((char*)&sizeX, 4);
            file.read((char*)&sizeY, 4);
            file.read((char*)&sizeZ, 4);
        }
        else if (id == "XYZI") {
            // Read the actual voxel data
            int numVoxels;
            file.read((char*)&numVoxels, 4);

            for (int i = 0; i < numVoxels && file; ++i) {
                unsigned char v[4]; // x, y, z, colorIndex
                file.read((char*)v, 4);

                int blockType = v[3];
                // Palette slots that collide with gameplay blocks: 1-4 are terrain (grass, dirt,
                // stone, sand), so they move to 36-39 (same colors, see default.comp); 30 and 32
                // (cavern glow, artifact) are swapped for similar leaf colors.
                if (blockType >= 1 && blockType <= 4) blockType += 35;
                else if (blockType == 30) blockType = 31;
                else if (blockType == 32) blockType = 28;

                // MagicaVoxel uses Z as the vertical "Up" axis, so swap Y and Z;
                // its Y is mirrored to keep the model's handedness in a Y-up world
                offsets.push_back({ (int)v[0] - sizeX / 2, (int)v[2], (sizeY - 1 - (int)v[1]) - sizeY / 2, blockType });
            }
        }
        else {
            // Skip unhandled chunks (like the RGBA palette)
            file.seekg(contentSize, std::ios::cur);
        }
    }

    return offsets;
}
