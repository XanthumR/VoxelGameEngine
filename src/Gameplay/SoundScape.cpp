#include "Gameplay/SoundScape.h"

#include "Simulation/BuildingTypes.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float NEAREST_ZOOM = 60.0f, FARTHEST_ZOOM = 1200.0f; // StrategyCamera's range (voxels)
// Each loop's loudness when it is full on (the recordings differ: sparse animal calls are quieter)
constexpr std::array<float, AMBIENCE_COUNT> GAINS = { 0.45f, 0.4f, 0.7f, 0.55f, 0.5f, 0.85f, 0.45f, 0.65f };

struct ModelSound {
    const char* model;
    Ambience ambience;
};
constexpr ModelSound MODEL_SOUNDS[] = {
    { "lumberjack", Ambience::Saw }, { "sawmill", Ambience::Saw }, { "window_makers", Ambience::Saw },
    { "iron_mine", Ambience::Smithy }, { "copper_mine", Ambience::Smithy }, { "gold_mine", Ambience::Smithy },
    { "furnace", Ambience::Smithy }, { "steelworks", Ambience::Smithy }, { "sewing_machine_factory", Ambience::Smithy },
    { "cannery", Ambience::Smithy }, { "brick_factory", Ambience::Smithy }, { "glassworks", Ambience::Smithy },
    { "spectacle_factory", Ambience::Smithy }, { "filament_factory", Ambience::Smithy }, { "light_bulb_factory", Ambience::Smithy },
    { "goldsmiths", Ambience::Smithy },
    { "flour_mill", Ambience::Mill }, { "rendering_works", Ambience::Mill }, { "soap_factory", Ambience::Mill },
    { "framework_knitter", Ambience::Mill }, { "clay_pit", Ambience::Mill }, { "sand_mine", Ambience::Mill },
    { "sheep_farm", Ambience::Animals }, { "pig_farm", Ambience::Animals }, { "cattle_farm", Ambience::Animals },
    { "sheepfold", Ambience::Animals }, { "pigsty", Ambience::Animals }, { "pasture", Ambience::Animals },
    { "slaughterhouse", Ambience::Animals },
    { "marketplace", Ambience::Town }, { "school", Ambience::Town }, { "variety_theatre", Ambience::Town },
    { "university", Ambience::Town }, { "bank", Ambience::Town }, { "bakery", Ambience::Town }, { "champagne_cellar", Ambience::Town },
    { "harbor", Ambience::Harbor }, { "fishery", Ambience::Harbor },
};

bool InUse(const GameObjectRegistry& objects, GameObjectId id) {
    switch (BUILDING_TYPES[objects.Building(id).type].role) {
    case BuildingRole::Residence: return objects.Residence(id).residents > 0;
    case BuildingRole::Producer: return objects.Production(id).status == ProducerStatus::Working;
    default: return true; // Services, storage, pens and fields, trading posts
    }
}

} // namespace

Ambience AmbienceOf(uint16_t buildingType) {
    const BuildingType& type = BUILDING_TYPES[buildingType];
    if (type.role == BuildingRole::Residence) return Ambience::Town;
    if (!type.modelName) return Ambience::Count;
    for (const ModelSound& sound : MODEL_SOUNDS) {
        if (std::strcmp(sound.model, type.modelName) == 0) return sound.ambience;
    }
    return Ambience::Count;
}

std::array<float, AMBIENCE_COUNT> AmbienceLevels(const GameObjectRegistry& objects, glm::vec2 focusColumn, float cameraDistance) {
    // The types' loops, found once
    static const std::array<Ambience, BUILDING_TYPES.size()> sounds = [] {
        std::array<Ambience, BUILDING_TYPES.size()> table{};
        for (uint16_t type = 0; type < BUILDING_TYPES.size(); type++) table[type] = AmbienceOf(type);
        return table;
    }();

    float closeness = std::clamp(1.0f - (cameraDistance - NEAREST_ZOOM) / (FARTHEST_ZOOM - NEAREST_ZOOM), 0.0f, 1.0f);
    float hearing = 100.0f + 0.4f * cameraDistance; // Voxels round the focus the buildings are heard from

    std::array<float, AMBIENCE_COUNT> sums{};
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id == INVALID_GAME_OBJECT) continue;
        Ambience ambience = sounds[objects.Building(id).type];
        if (ambience == Ambience::Count || !InUse(objects, id)) continue;
        const VoxelAnchorComponent& anchor = objects.Anchor(id);
        glm::vec2 middle = glm::vec2(anchor.origin.x, anchor.origin.z) + glm::vec2(anchor.footprint) * 0.5f;
        float near = 1.0f - glm::length(middle - focusColumn) / hearing;
        if (near > 0.0f) sums[(size_t)ambience] += near * near;
    }

    std::array<float, AMBIENCE_COUNT> levels{};
    for (int i = 0; i < AMBIENCE_COUNT; i++) {
        // Softly capped: a few buildings near are about full, more do not get louder
        levels[i] = GAINS[i] * (1.0f - std::exp(-0.8f * sums[i])) * (0.35f + 0.65f * closeness);
    }
    levels[(size_t)Ambience::Sea] = GAINS[(size_t)Ambience::Sea] * (0.7f + 0.3f * (1.0f - closeness));
    levels[(size_t)Ambience::Wind] = GAINS[(size_t)Ambience::Wind] * (0.15f + 0.85f * (1.0f - closeness));
    return levels;
}
