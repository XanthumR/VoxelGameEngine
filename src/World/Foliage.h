#pragma once

#include "World/VoxModel.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// The foliage of the islands, as in Anno 1800: trees (the wood lumberjacks fell), bushes, rocks,
// logs and flowers. Each kind is one model in assets/foliage (tools/foliage/generate.py), grows in
// one climate, and is picked by a weight that differs in forests and in meadows. The terrain
// generator places them (TerrainGenerator::FoliageAt); a building or road clears any of them.
enum class FoliageClass : uint8_t { Tree, Bush, Rock, Log, Flower };
enum class Climate : uint8_t { Temperate, Tropical, Both };

struct FoliageKind {
    const char* file; // assets/foliage/<file>.vox
    FoliageClass kind;
    Climate climate;
    uint8_t forestWeight, meadowWeight;
};

constexpr std::array<FoliageKind, 34> FOLIAGE_KINDS = { {
    // Old World trees: spruce and beech deep in the forests, oak and birch at the edges and alone
    { "oak_1", FoliageClass::Tree, Climate::Temperate, 2, 4 },
    { "oak_2", FoliageClass::Tree, Climate::Temperate, 2, 4 },
    { "beech_1", FoliageClass::Tree, Climate::Temperate, 4, 1 },
    { "beech_2", FoliageClass::Tree, Climate::Temperate, 4, 1 },
    { "birch_1", FoliageClass::Tree, Climate::Temperate, 2, 3 },
    { "birch_2", FoliageClass::Tree, Climate::Temperate, 2, 3 },
    { "spruce_1", FoliageClass::Tree, Climate::Temperate, 4, 1 },
    { "spruce_2", FoliageClass::Tree, Climate::Temperate, 4, 1 },
    // New World trees
    { "palm_1", FoliageClass::Tree, Climate::Tropical, 4, 4 },
    { "palm_2", FoliageClass::Tree, Climate::Tropical, 4, 4 },
    { "palm_3", FoliageClass::Tree, Climate::Tropical, 3, 4 },
    { "jungle_tree_1", FoliageClass::Tree, Climate::Tropical, 1, 0 },
    // Ground cover: ferns, logs and stumps under the trees, flowers, bushes and rocks in the open
    { "bush_1", FoliageClass::Bush, Climate::Temperate, 2, 4 },
    { "bush_2", FoliageClass::Bush, Climate::Temperate, 1, 3 },
    { "bush_3", FoliageClass::Bush, Climate::Temperate, 1, 3 },
    { "boulder_1", FoliageClass::Rock, Climate::Both, 1, 2 },
    { "boulder_2", FoliageClass::Rock, Climate::Both, 2, 2 },
    { "boulder_3", FoliageClass::Rock, Climate::Both, 1, 2 },
    { "log_1", FoliageClass::Log, Climate::Temperate, 3, 1 },
    { "log_2", FoliageClass::Log, Climate::Temperate, 3, 1 },
    { "stump_1", FoliageClass::Log, Climate::Temperate, 3, 1 },
    { "flowers_1", FoliageClass::Flower, Climate::Temperate, 0, 5 },
    { "flowers_2", FoliageClass::Flower, Climate::Temperate, 0, 5 },
    { "flowers_3", FoliageClass::Flower, Climate::Temperate, 0, 5 },
    { "fern_1", FoliageClass::Flower, Climate::Temperate, 6, 1 },
    { "fern_2", FoliageClass::Flower, Climate::Temperate, 6, 1 },
    { "banana_1", FoliageClass::Bush, Climate::Tropical, 3, 2 },
    { "monstera_1", FoliageClass::Bush, Climate::Tropical, 4, 2 },
    { "monstera_2", FoliageClass::Bush, Climate::Tropical, 4, 2 },
    { "agave_1", FoliageClass::Bush, Climate::Tropical, 0, 4 },
    { "palm_log_1", FoliageClass::Log, Climate::Tropical, 2, 1 },
    { "driftwood_1", FoliageClass::Log, Climate::Tropical, 1, 2 },
    { "hibiscus_1", FoliageClass::Flower, Climate::Tropical, 1, 5 },
    { "tropical_fern_1", FoliageClass::Flower, Climate::Tropical, 6, 2 },
} };

// A model of FoliageLibrary: a kind in one of four quarter turns
constexpr int FoliageModelIndex(int kind, int rotation) { return kind * 4 + rotation; }
constexpr int FoliageKindOf(int model) { return model / 4; }
inline bool IsTreeModel(int model) { return FOLIAGE_KINDS[FoliageKindOf(model)].kind == FoliageClass::Tree; }

// Every kind's model in its four quarter turns
class FoliageLibrary {
public:
    // Loads assets/foliage from the asset directory; false (and empty) if a model is missing
    bool Load(const std::string& assetDirectory);
    bool IsEmpty() const { return m_Models.empty(); }
    const VoxModel& Model(int model) const { return m_Models[model]; }
    int Reach() const { return m_Reach; }   // Columns the widest model reaches from its root along x or z
    int Height() const { return m_Height; } // The tallest model's top above its root

private:
    std::vector<VoxModel> m_Models;
    int m_Reach = 0, m_Height = 0;
};
