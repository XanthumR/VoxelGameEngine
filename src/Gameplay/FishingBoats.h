#pragma once

#include "Gameplay/Figure.h"
#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class BuildingModelLibrary;
class TerrainGenerator;

// The fishing boats of the fisheries. Purely visual: each fishery whose model has a boat berth
// (palette index 101 in its .vox) has a boat that lies at the end of its dock, sails out to sea
// along the dock's direction, fishes a while, and comes back, once per TRIP_SECONDS. The route is
// cut short where the sea gets shallow, so a boat never sails onto land.
class FishingBoats {
public:
    static constexpr int MAX_BOATS = 64;
    static constexpr float TRIP_SECONDS = 30.0f;
    static constexpr float MOORED_SECONDS = 4.0f;
    static constexpr float FISHING_SECONDS = 4.0f;
    static constexpr int MAX_DISTANCE = 80;   // Voxels out from the berth
    static constexpr int BOAT_HALF_LENGTH = 5; // The hull reaches this far from its center (and a voxel to spare)

    FishingBoats();

    void Update(float deltaTime, const GameObjectRegistry& objects, const BuildingModelLibrary& models, TerrainGenerator& terrain);
    void AppendFigures(std::vector<Figure>& out) const;
    size_t BoatCount() const { return m_Boats.size(); }

    // Voxels the boat can sail from start along direction before the water gets too shallow
    static int RouteLength(TerrainGenerator& terrain, glm::ivec2 start, glm::ivec2 direction, int maxDistance);
    // How far out along the route a boat is at this time of its trip
    static float OffsetAt(float tripTime, int routeLength);

private:
    struct Boat {
        GameObjectId fishery;
        glm::ivec3 start;     // Center of the moored boat (its waterline)
        glm::ivec2 direction; // Out to sea
        int routeLength;
        float time;           // Seconds into the current trip
    };

    std::vector<Boat> m_Boats;
};
