#include <iostream>
#include <fstream>
#include <vector>
#include <string>

struct Offset {
    int x, y, z, blockType;
};

std::vector<Offset> loadVoxToOffsets(const std::string& filepath) {
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

    if (std::string(magic, 4) != "VOX ") {
        std::cerr << "Not a valid MagicaVoxel file!" << std::endl;
        return offsets;
    }

    // 2. Parse chunks
    while (!file.eof()) {
        char chunkId[4];
        int contentSize, childrenSize;

        file.read(chunkId, 4);
        if (file.eof()) break;

        file.read((char*)&contentSize, 4);
        file.read((char*)&childrenSize, 4);

        std::string id(chunkId, 4);

        if (id == "MAIN" || id == "PACK") {
            // Container chunks, keep reading forward
            continue;
        }
        else if (id == "SIZE") {
            // Read dimensions (useful if you want to center the model)
            int sizeX, sizeY, sizeZ;
            file.read((char*)&sizeX, 4);
            file.read((char*)&sizeY, 4);
            file.read((char*)&sizeZ, 4);
        }
        else if (id == "XYZI") {
            // Read the actual voxel data
            int numVoxels;
            file.read((char*)&numVoxels, 4);

            for (int i = 0; i < numVoxels; ++i) {
                unsigned char x, y, z, colorIndex;
                file.read((char*)&x, 1);
                file.read((char*)&y, 1);
                file.read((char*)&z, 1);
                file.read((char*)&colorIndex, 1);

                // Note: MagicaVoxel uses Z as the vertical "Up" axis.
                // Depending on your coordinate system (e.g., OpenGL often uses Y as Up),
                // you might want to swap Y and Z here.
                offsets.push_back({ (int)x, (int)z, (int)y, (int)5 });
            }
        }
        else {
            // Skip unhandled chunks (like the RGBA palette)
            file.seekg(contentSize, std::ios::cur);
        }
    }

    

    return offsets;
}