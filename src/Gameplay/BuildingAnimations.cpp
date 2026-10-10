#include "Gameplay/BuildingAnimations.h"

#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/Placement.h"
#include "World/BuildingModel.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace {

constexpr float HALF_PI = 1.5707963f;

// A point in a building's frame (continuous u, v) to world columns, for the building's rotation
// (the continuous form of RotateToFootprint)
glm::vec2 ToWorld(glm::vec2 uv, uint8_t rotation, glm::vec2 size, const VoxelAnchorComponent& anchor) {
    glm::vec2 column;
    switch (rotation & 3) {
    case 0: column = uv; break;
    case 1: column = glm::vec2(size.x - uv.y, uv.x); break;
    case 2: column = glm::vec2(size.x - uv.x, size.y - uv.y); break;
    default: column = glm::vec2(uv.y, size.y - uv.x); break;
    }
    return column + glm::vec2((float)anchor.origin.x, (float)anchor.origin.z);
}

} // namespace

BuildingAnimations::BuildingAnimations() {
    m_Clocks.assign(GameObjectRegistry::MAX_OBJECTS, 0.0f);
}

bool BuildingAnimations::Load(const std::string& directory, std::vector<VoxelObjectModel>& models) {
    m_Parts.clear();
    std::ifstream manifest(directory + "/parts.txt");
    if (!manifest) {
        std::cout << "No moving parts: " << directory << "/parts.txt is missing" << std::endl;
        return false;
    }
    std::map<std::string, int> loaded; // Part file -> model index
    std::string line;
    while (std::getline(manifest, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream fields(line);
        std::string building, file, motion, axis;
        int u = 0, v = 0, y = 0;
        Part part;
        if (!(fields >> building >> file >> u >> v >> y >> motion >> axis >> part.a >> part.b)) {
            std::cout << "parts.txt: can't read \"" << line << "\"" << std::endl;
            continue;
        }
        int type = -1;
        for (int t = 0; t < (int)BUILDING_TYPES.size(); t++) {
            if (BUILDING_TYPES[t].modelName && building == BUILDING_TYPES[t].modelName) type = t;
        }
        if (type < 0) {
            std::cout << "parts.txt: no building " << building << std::endl;
            continue;
        }

        auto found = loaded.find(file);
        if (found == loaded.end()) {
            BuildingModel model;
            if (!model.Load(directory + "/" + file + ".vox")) {
                std::cout << "parts.txt: can't load " << file << ".vox" << std::endl;
                continue;
            }
            VoxelObjectModel object; // The same voxel order as building models
            object.size = glm::ivec3(model.width, model.height, model.depth);
            object.ids = model.ids;
            found = loaded.emplace(file, (int)models.size()).first;
            models.push_back(std::move(object));
        }
        part.model = found->second;
        part.pivotHeight = models[part.model].size.y * 0.5f;
        part.buildingType = (uint16_t)type;
        part.motion = motion == "spin" ? Motion::Spin : motion == "swing" ? Motion::Swing : motion == "slide" ? Motion::Slide : Motion::Wander;
        part.axis = axis == "across" ? Axis::Across : axis == "forward" ? Axis::Forward : Axis::Up;
        // Voxel middles; a wandering animal stands on its ground
        part.pivot = glm::vec3(u + 0.5f, v + 0.5f, part.motion == Motion::Wander ? (float)y : y + 0.5f);
        part.phase = 1.7f * (float)m_Parts.size();
        m_Parts.push_back(part);
    }

    std::stable_sort(m_Parts.begin(), m_Parts.end(), [](const Part& a, const Part& b) { return a.buildingType < b.buildingType; });
    m_FirstPart.assign(BUILDING_TYPES.size() + 1, (uint32_t)m_Parts.size());
    for (size_t i = m_Parts.size(); i-- > 0;) m_FirstPart[m_Parts[i].buildingType] = (uint32_t)i;
    for (size_t t = BUILDING_TYPES.size(); t-- > 0;) m_FirstPart[t] = std::min(m_FirstPart[t], m_FirstPart[t + 1]);
    return true;
}

bool BuildingAnimations::InUse(const GameObjectRegistry& objects, GameObjectId id) {
    if (BUILDING_TYPES[objects.Building(id).type].role != BuildingRole::Producer) return true;
    return objects.Production(id).status == ProducerStatus::Working;
}

void BuildingAnimations::Update(float gameDelta, const GameObjectRegistry& objects) {
    for (uint32_t slot = 0; slot < objects.SlotCount(); slot++) {
        GameObjectId id = objects.IdAtSlot(slot);
        if (id != INVALID_GAME_OBJECT && (m_AlwaysInUse || InUse(objects, id))) m_Clocks[slot] += gameDelta;
    }
}

VoxelObject BuildingAnimations::Place(const Part& part, const BuildingComponent& building, const VoxelAnchorComponent& anchor, float time, float phase) {
    const BuildingType& type = BUILDING_TYPES[building.type];
    glm::vec2 size = glm::vec2(FootprintColumns(type, building.rotation));
    float ground = (float)(BUILD_GROUND_Y - type.belowGround); // The model's y = 0
    float buildingYaw = -(float)building.rotation * HALF_PI;   // Its v (forward) in the world
    phase += part.phase;

    VoxelObject object;
    object.model = part.model;
    if (part.motion == Motion::Wander) {
        // Two slow sines out of step make a meandering path; the eased time (speed 1 + 0.8 cos)
        // makes the animal nearly stop now and then, to graze
        float t = time * 0.3f + phase;
        auto at = [&](float s) {
            return glm::vec2(part.a * std::sin(0.71f * s + phase * 1.3f), part.b * std::sin(0.53f * s + phase * 2.1f));
        };
        float s = t + 0.8f * std::sin(t);
        glm::vec2 here = at(s), ahead = at(s + 0.05f);
        glm::vec2 position = ToWorld(glm::vec2(part.pivot) + here, building.rotation, size, anchor);
        glm::vec2 heading = ToWorld(glm::vec2(part.pivot) + ahead, building.rotation, size, anchor) - position;
        bool walking = glm::length(ahead - here) > 0.02f;
        object.position = glm::vec3(position.x, ground + part.pivot.z + (walking && std::fmod(s * 4.0f, 1.0f) < 0.5f ? 1.0f : 0.0f), position.y);
        object.yaw = std::atan2(heading.x, heading.y); // Yaw 0 faces +z, a quarter turn +x
        return object;
    }

    float angle = 0.0f, slide = 0.0f;
    switch (part.motion) {
    case Motion::Spin: angle = part.a * time + phase; break;
    case Motion::Swing: angle = part.b * std::sin(part.a * time + phase); break;
    default: slide = part.b * 0.5f * (1.0f - std::cos(part.a * time + phase)); break;
    }
    glm::vec3 pivot = part.pivot; // (u, v, height)
    if (part.axis == Axis::Across) pivot.x += slide;
    if (part.axis == Axis::Forward) pivot.y += slide;
    if (part.axis == Axis::Up) pivot.z += slide;
    object.yaw = buildingYaw + (part.axis == Axis::Up ? angle : 0.0f);
    object.pitch = part.axis == Axis::Across ? angle : 0.0f;
    object.roll = part.axis == Axis::Forward ? angle : 0.0f;

    // The model turns about its bottom middle (VoxelRenderer): move it so its middle stays on the pivot
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), object.yaw, glm::vec3(0, 1, 0));
    rotation = glm::rotate(rotation, -object.pitch, glm::vec3(1, 0, 0));
    rotation = glm::rotate(rotation, object.roll, glm::vec3(0, 0, 1));
    glm::vec2 column = ToWorld(glm::vec2(pivot), building.rotation, size, anchor);
    object.position = glm::vec3(column.x, ground + pivot.z, column.y) - glm::vec3(rotation * glm::vec4(0.0f, part.pivotHeight, 0.0f, 0.0f));
    return object;
}

void BuildingAnimations::AppendBuilding(const GameObjectRegistry& objects, uint32_t slot, GameObjectId id, glm::vec2 focusColumn, float maxDistance,
    int modelBase, std::vector<VoxelObject>& out) const {
    const BuildingComponent& building = objects.Building(id);
    uint32_t first = m_FirstPart[building.type], last = m_FirstPart[building.type + 1];
    if (first == last) return;
    const VoxelAnchorComponent& anchor = objects.Anchor(id);
    glm::vec2 center = glm::vec2(anchor.origin.x, anchor.origin.z) + glm::vec2(anchor.footprint) * 0.5f;
    if (glm::length(center - focusColumn) > maxDistance) return;

    float time = m_Clocks[slot];
    float phase = (float)(id % 997) * 0.618f;  // Neighbours out of step
    for (uint32_t i = first; i < last && out.size() < out.capacity(); i++) {
        VoxelObject object = Place(m_Parts[i], building, anchor, time, phase);
        object.model += modelBase;
        out.push_back(object); // Within the caller's reserve
    }
}
