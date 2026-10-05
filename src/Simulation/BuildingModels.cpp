#include "Simulation/BuildingModels.h"

#include "Simulation/BuildingLook.h"

#include <algorithm>
#include <iostream>

int BuildingModelLibrary::LoadAll(const std::string& directory) {
    int loaded = 0;
    for (size_t type = 0; type < BUILDING_TYPES.size(); type++) {
        const BuildingType& building = BUILDING_TYPES[type];
        m_Models[type].clear();
        if (!building.modelName) continue;
        for (int n = 1; n <= MAX_VARIANTS; n++) {
            std::string path = directory + "/" + building.modelName + "_" + std::to_string(n) + ".vox";
            BuildingModel model;
            if (!model.Load(path)) continue;
            if (!Fits(model, building)) {
                std::cerr << "Building model " << path << " is " << model.width << " x " << model.depth << " x " << model.height
                          << "; " << building.name << " needs " << building.footprintWidth * TILE_SIZE << " x "
                          << building.footprintDepth * TILE_SIZE << " x at most " << BuildingVolumeHeight(building) << ". Skipped." << std::endl;
                continue;
            }
            m_Models[type].push_back(std::move(model));
            loaded++;
        }
        if (m_Models[type].empty()) std::cerr << "No models for " << building.name << "; using the procedural look." << std::endl;
    }
    return loaded;
}

bool BuildingModelLibrary::Fits(const BuildingModel& model, const BuildingType& type) {
    return model.width == type.footprintWidth * TILE_SIZE && model.depth == type.footprintDepth * TILE_SIZE &&
           model.height >= 1 && model.height <= BuildingVolumeHeight(type);
}

const BuildingModel* BuildingModelLibrary::Model(uint16_t type, uint8_t variant) const {
    const std::vector<BuildingModel>& models = m_Models[type];
    return models.empty() ? nullptr : &models[variant % models.size()];
}

uint8_t BuildingModelLibrary::PickVariant(uint16_t type, uint32_t seed) const {
    int count = VariantCount(type);
    if (count <= 1) return 0;
    uint32_t hash = seed * 2654435761u; // Knuth's multiplicative hash spreads consecutive IDs
    return (uint8_t)((hash >> 16) % (uint32_t)count);
}

void BuildingModelLibrary::BuildVoxels(uint16_t type, uint8_t variant, uint8_t rotation, std::vector<uint8_t>& ids) const {
    const BuildingType& building = BUILDING_TYPES[type];
    const std::vector<BuildingModel>& models = m_Models[type];
    const glm::ivec2 size = FootprintColumns(building, rotation);
    const int height = BuildingVolumeHeight(building);
    if (models.empty()) {
        // The procedural look stands on the ground: put it above the below-ground layers
        BuildLook(building, rotation, m_LookScratch);
        ids.assign((size_t)size.x * size.y * height, Block::AIR);
        std::copy(m_LookScratch.begin(), m_LookScratch.end(), ids.begin() + (size_t)size.x * size.y * building.belowGround);
        return;
    }

    const BuildingModel& model = models[variant % models.size()];
    ids.assign((size_t)size.x * size.y * height, Block::AIR);
    for (int y = 0; y < model.height; y++) {
        for (int v = 0; v < model.depth; v++) {
            for (int u = 0; u < model.width; u++) {
                uint8_t id = model.At(u, v, y);
                if (id == Block::AIR) continue;
                glm::ivec2 column = RotateToFootprint(u, v, rotation, size);
                ids[(size_t)column.x + (size_t)size.x * ((size_t)column.y + (size_t)size.y * (size_t)y)] = id;
            }
        }
    }
}
