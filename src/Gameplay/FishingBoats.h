#pragma once

#include "Rendering/VoxelObject.h"
#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class BuildingModelLibrary;
class TerrainGenerator;

// The fishing boats of the fisheries. Purely visual: each fishery whose model has a boat berth
// (palette index 101 in its .vox) has a boat that lies at the end of its dock, sails out to sea
// at an angle that changes every trip, fishes a while, turns around and comes back: one trip per
// fish, at the pace the fishery works (TRIP_SECONDS is its cycle at full productivity). When the
// fishery stops, the boat finishes its trip and stays moored. The route is cut short where the sea
// gets shallow, so a boat never sails onto land. Boats are voxel objects: they turn smoothly and
// ride the ocean's waves.
class FishingBoats {
public:
    static constexpr int MAX_BOATS = 64;
    static constexpr float TRIP_SECONDS = 30.0f;
    static constexpr float MOORED_SECONDS = 4.0f;
    static constexpr float FISHING_SECONDS = 4.0f;
    static constexpr float TURN_SECONDS = 2.0f;  // Turning out of the berth at the start of a trip
    static constexpr int MAX_DISTANCE = 80;      // Voxels out from the berth
    static constexpr int MAX_TRIP_ANGLE = 35;    // Degrees a trip may head off the dock's direction
    static constexpr int BOAT_HALF_LENGTH = 9;   // The hull reaches this far from its center

    FishingBoats();

    // The boat's two looks (sail furled, sail set), registered with the renderer
    static VoxelObjectModel BuildModel(bool sailSet);
    void SetModels(int moored, int sailing) { m_MooredModel = moored; m_SailingModel = sailing; }

    void Update(float deltaTime, const GameObjectRegistry& objects, const BuildingModelLibrary& models, TerrainGenerator& terrain);
    void AppendObjects(std::vector<VoxelObject>& out) const;
    size_t BoatCount() const { return m_Boats.size(); }

    // Voxels the boat can sail from start along direction (unit length) before the water gets too shallow
    static int RouteLength(TerrainGenerator& terrain, glm::vec2 start, glm::vec2 direction, int maxDistance);
    // How far out along the route a boat is at this time of its trip
    static float OffsetAt(float tripTime, int routeLength);
    // Which way the bow points at this time of the trip (yaw, radians): it turns from its moored
    // heading to the trip's, turns around while fishing, and comes back bow first
    static float HeadingAt(float tripTime, float mooredHeading, float tripHeading);

private:
    struct Boat {
        GameObjectId fishery;
        glm::vec2 start;      // Center of the moored boat
        float dockHeading;    // Out to sea along the dock
        float mooredHeading;  // How it lies at the berth (bow toward land after a trip)
        float tripHeading;    // This trip's way out
        int routeLength;
        uint32_t trip;
        float time;           // Seconds into the current trip
    };

    void StartTrip(Boat& boat, TerrainGenerator& terrain);

    std::vector<Boat> m_Boats;
    int m_MooredModel = 0, m_SailingModel = 0;
};
