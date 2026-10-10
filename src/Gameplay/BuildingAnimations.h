#pragma once

#include "Rendering/VoxelObject.h"
#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

// Small moving parts that make the town feel alive: windmill sails and a mine's wheel turn, signs
// and flags sway, hoists go up and down, animals walk about their pens. Each part is a voxel
// object (its own model, assets/buildings/parts/<name>.vox) placed on a building every frame;
// assets/buildings/parts/parts.txt (written by tools/building_models/generate.py) says which part
// sits where on which building and how it moves. Each building keeps its own clock, which runs in
// game time while the building is in use (a producer only while it works), so parts stop where
// they are: a windmill's sails stand still while the mill has no grain, and everything stops when
// the game is paused.
class BuildingAnimations {
public:
    enum class Motion : uint8_t { Spin, Swing, Slide, Wander };
    enum class Axis : uint8_t { Across, Up, Forward }; // The building's u, y and v

    struct Part {
        uint16_t buildingType = 0;
        int model = 0;          // Index into the models Load returned
        glm::vec3 pivot{ 0.0f }; // Building frame: x = u, y = v, z = height (wandering animals: their ground)
        float pivotHeight = 0;  // The model's middle above its bottom face
        Motion motion = Motion::Spin;
        Axis axis = Axis::Up;
        float a = 0, b = 0;     // Per motion, see parts.txt
        float phase = 0;        // Keeps parts of one building (the animals of a pen) out of step
    };

    // Reads the manifest and the part models; models gets one VoxelObjectModel per part file, in
    // the order Part::model counts. False (and no parts) when the manifest is missing.
    bool Load(const std::string& directory, std::vector<VoxelObjectModel>& models);

    BuildingAnimations();

    // Runs the clocks of the buildings in use
    void Update(float gameDelta, const GameObjectRegistry& objects);
    // Every building's parts move, in use or not (--showcase, where nothing has workers)
    void SetAlwaysInUse(bool always) { m_AlwaysInUse = always; }

    // The parts of the buildings within maxDistance columns of the focus; hidden(id) leaves a
    // building out (one being built or carried). modelBase: where the part models start in the renderer.
    template <typename Hidden>
    void AppendObjects(const GameObjectRegistry& objects, glm::vec2 focusColumn, float maxDistance, int modelBase, std::vector<VoxelObject>& out,
        Hidden hidden) const {
        for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
            GameObjectId id = objects.IdAtSlot(slot);
            if (id != INVALID_GAME_OBJECT && !hidden(id)) AppendBuilding(objects, slot, id, focusColumn, maxDistance, modelBase, out);
        }
    }

    // Whether a building's parts move now (a producer only while it works)
    static bool InUse(const GameObjectRegistry& objects, GameObjectId id);
    // Where a part of a building is at this time on its clock (pure, for tests)
    static VoxelObject Place(const Part& part, const BuildingComponent& building, const VoxelAnchorComponent& anchor, float time, float phase);

    const std::vector<Part>& Parts() const { return m_Parts; }

private:
    void AppendBuilding(const GameObjectRegistry& objects, uint32_t slot, GameObjectId id, glm::vec2 focusColumn, float maxDistance, int modelBase,
        std::vector<VoxelObject>& out) const;

    std::vector<Part> m_Parts;           // Sorted by building type
    std::vector<uint32_t> m_FirstPart;   // Per building type: its first part (parts are sorted by type); one more at the end
    std::vector<float> m_Clocks;         // Per object slot: seconds the building has been in use
    bool m_AlwaysInUse = false;
};
