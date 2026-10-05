#pragma once

#include "Simulation/BuildingTypes.h"
#include "World/BuildingModel.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// The .vox models of every building type (assets/buildings/<model name>_<n>.vox, n = 1, 2, ...),
// and the voxels a placed building is stamped with: its model variant, rotated, or the procedural
// look (BuildLook) when the type has no usable model.
class BuildingModelLibrary {
public:
    static constexpr int MAX_VARIANTS = 4;

    // Loads every model in directory; missing files are fine, wrong-sized ones are skipped with a
    // warning. Returns the number of models loaded.
    int LoadAll(const std::string& directory);

    int VariantCount(uint16_t type) const { return (int)m_Models[type].size(); }

    // Model variant for a new building: spread over the variants by its ID, so neighbours differ
    uint8_t PickVariant(uint16_t type, uint32_t seed) const;

    // Fills ids with the building's volume (FootprintColumns x BuildingHeight, x fastest, then z,
    // then y), AIR where empty: the variant's model if the type has models, else BuildLook
    void BuildVoxels(uint16_t type, uint8_t variant, uint8_t rotation, std::vector<uint8_t>& ids) const;

    // Checks a model against its type: footprint x TILE_SIZE across, at most the type's height
    static bool Fits(const BuildingModel& model, const BuildingType& type);

private:
    std::array<std::vector<BuildingModel>, BUILDING_TYPES.size()> m_Models;
};
