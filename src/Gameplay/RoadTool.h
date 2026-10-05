#pragma once

#include "Gameplay/Picking.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

struct GLFWwindow;
class Simulation;
class VoxelWorld;
class WorldEditor;

// Building and removing roads (strategy camera, "Road" selected in the build menu). Press the left
// button on a tile and drag: the road follows an L-shaped path and is built on release (invalid
// tiles are skipped). Right-drag removes road the same way; a right click that removes nothing asks
// to leave road mode.
class RoadTool {
public:
    static constexpr size_t MAX_PATH_TILES = 1024;

    RoadTool(const VoxelWorld& world, WorldEditor& editor, Simulation& simulation);

    // Once per frame while road mode is on
    void Update(GLFWwindow* window, const PickResult& hover, bool mouseFree);
    void Cancel(); // Drops a drag in progress (road mode switched off)

    // True once after a right click that removed nothing: the caller leaves road mode
    bool ConsumeCancelRequest();

    // One tile of road: the grass layer becomes road and the space above is cleared. False if the
    // tile is not valid (see ValidateRoadTile) / not road.
    bool Build(glm::ivec2 tile);
    bool Demolish(glm::ivec2 tile);

    // Preview: the dragged path (or the hovered tile), and per tile whether it can be built
    const std::vector<glm::ivec2>& PathTiles() const { return m_Path; }
    const std::vector<uint8_t>& PathValid() const { return m_PathValid; }
    bool Removing() const { return m_Removing; }
    bool Dragging() const { return m_Dragging; }
    uint32_t PreviewRevision() const { return m_PreviewRevision; } // Changes when the preview does

private:
    // Writes a road tile's surface: dirt, with a stone kerb along each side that has no road next to it
    void PaintTile(glm::ivec2 tile);
    void PaintNeighbours(glm::ivec2 tile);

    void SetPath(glm::ivec2 start, glm::ivec2 end, bool removing);
    void ClearPath();

    const VoxelWorld& m_World;
    WorldEditor& m_Editor;
    Simulation& m_Simulation;

    std::vector<glm::ivec2> m_Path;
    std::vector<uint8_t> m_PathValid;
    glm::ivec2 m_Start = glm::ivec2(0), m_End = glm::ivec2(0);
    bool m_HasPath = false;
    bool m_Dragging = false;
    bool m_Removing = false;
    uint32_t m_PathRoadRevision = 0; // Road network revision the path validity was computed for
    uint32_t m_PreviewRevision = 0;
    bool m_CancelRequested = false;
    bool m_LeftWasDown = false, m_RightWasDown = false;
    std::vector<uint8_t> m_PaintBuffer; // One tile's surface, reused
};
