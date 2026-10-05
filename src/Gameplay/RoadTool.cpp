#include "Gameplay/RoadTool.h"

#include "Simulation/BuildingTypes.h"
#include "Simulation/Simulation.h"
#include "World/BlockTypes.h"
#include "World/WorldEditor.h"

#include <GLFW/glfw3.h>

RoadTool::RoadTool(const VoxelWorld& world, WorldEditor& editor, Simulation& simulation)
    : m_World(world), m_Editor(editor), m_Simulation(simulation) {
    m_Path.reserve(MAX_PATH_TILES * 2);
    m_PathValid.reserve(MAX_PATH_TILES * 2);
    m_PaintBuffer.reserve(TILE_SIZE * TILE_SIZE);
}

void RoadTool::Update(GLFWwindow* window, const PickResult& hover, bool mouseFree) {
    bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool leftPressed = leftDown && !m_LeftWasDown;
    bool rightPressed = rightDown && !m_RightWasDown;
    m_LeftWasDown = leftDown;
    m_RightWasDown = rightDown;

    bool hasTile = hover.hit;
    glm::ivec2 hoverTile = hasTile ? glm::ivec2(ColumnToTile(hover.voxel.x), ColumnToTile(hover.voxel.z)) : m_End;

    if (!m_Dragging) {
        // Start a drag on a press over the world (not over the UI)
        if (mouseFree && hasTile && (leftPressed || rightPressed)) {
            m_Dragging = true;
            m_Start = hoverTile;
            SetPath(m_Start, hoverTile, rightPressed);
        } else if (mouseFree && hasTile) {
            SetPath(hoverTile, hoverTile, false); // Hover preview: the tile a click would start on
        } else if (m_HasPath) {
            ClearPath();
        }
        return;
    }

    // Dragging: follow the cursor, apply on release of the button that started it
    bool buttonDown = m_Removing ? rightDown : leftDown;
    if (buttonDown) {
        SetPath(m_Start, hoverTile, m_Removing);
        return;
    }

    if (m_Removing) {
        int removed = 0;
        for (const glm::ivec2& tile : m_Path) removed += Demolish(tile) ? 1 : 0;
        if (removed == 0 && m_Path.size() == 1) m_CancelRequested = true;
    } else {
        for (const glm::ivec2& tile : m_Path) Build(tile);
    }
    m_Dragging = false;
    ClearPath();
}

void RoadTool::Cancel() {
    m_Dragging = false;
    ClearPath();
}

bool RoadTool::ConsumeCancelRequest() {
    bool requested = m_CancelRequested;
    m_CancelRequested = false;
    return requested;
}

void RoadTool::SetPath(glm::ivec2 start, glm::ivec2 end, bool removing) {
    uint32_t roadRevision = m_Simulation.Roads().Revision();
    if (m_HasPath && start == m_Start && end == m_End && removing == m_Removing && roadRevision == m_PathRoadRevision) return;

    m_Start = start;
    m_End = end;
    m_Removing = removing;
    m_HasPath = true;
    m_PathRoadRevision = roadRevision;

    // Clamp very long drags so the path stays within the reserved buffers
    glm::ivec2 clampedEnd = start + glm::clamp(end - start, glm::ivec2(-(int)MAX_PATH_TILES / 2), glm::ivec2((int)MAX_PATH_TILES / 2));
    MakeLPath(start, clampedEnd, m_Path);

    PlacementContext context = m_Simulation.MakePlacementContext(m_World);
    m_PathValid.clear();
    for (const glm::ivec2& tile : m_Path) {
        bool valid = removing ? m_Simulation.Roads().IsRoad(tile) : ValidateRoadTile(context, tile).error == PlacementError::None;
        m_PathValid.push_back(valid ? 1 : 0);
    }
    m_PreviewRevision++;
}

void RoadTool::ClearPath() {
    if (!m_HasPath && m_Path.empty()) return;
    m_HasPath = false;
    m_Path.clear();
    m_PathValid.clear();
    m_PreviewRevision++;
}

bool RoadTool::Build(glm::ivec2 tile) {
    PlacementContext context = m_Simulation.MakePlacementContext(m_World);
    if (ValidateRoadTile(context, tile).error != PlacementError::None) return false;
    m_Simulation.Roads().Add(tile);

    glm::ivec3 minColumn(tile.x * TILE_SIZE, BUILD_GROUND_Y - 1, tile.y * TILE_SIZE);
    m_Editor.FillBox(minColumn + glm::ivec3(0, 1, 0), glm::ivec3(TILE_SIZE, ROAD_CLEARANCE, TILE_SIZE), Block::AIR); // Grass tufts
    PaintTile(tile);
    PaintNeighbours(tile); // Their kerb on this side goes
    return true;
}

void RoadTool::PaintTile(glm::ivec2 tile) {
    const RoadNetwork& roads = m_Simulation.Roads();
    if (!roads.IsRoad(tile)) return;
    bool west = roads.IsRoad(tile + glm::ivec2(-1, 0)), east = roads.IsRoad(tile + glm::ivec2(1, 0));
    bool north = roads.IsRoad(tile + glm::ivec2(0, -1)), south = roads.IsRoad(tile + glm::ivec2(0, 1));

    m_PaintBuffer.assign(TILE_SIZE * TILE_SIZE, Block::ROAD_DIRT);
    for (int z = 0; z < TILE_SIZE; z++) {
        for (int x = 0; x < TILE_SIZE; x++) {
            bool kerb = (x == 0 && !west) || (x == TILE_SIZE - 1 && !east) || (z == 0 && !north) || (z == TILE_SIZE - 1 && !south);
            if (kerb) m_PaintBuffer[(size_t)z * TILE_SIZE + x] = Block::ROAD_EDGE;
        }
    }
    m_Editor.WriteBox(glm::ivec3(tile.x * TILE_SIZE, BUILD_GROUND_Y - 1, tile.y * TILE_SIZE), glm::ivec3(TILE_SIZE, 1, TILE_SIZE), m_PaintBuffer);
}

void RoadTool::PaintNeighbours(glm::ivec2 tile) {
    PaintTile(tile + glm::ivec2(-1, 0));
    PaintTile(tile + glm::ivec2(1, 0));
    PaintTile(tile + glm::ivec2(0, -1));
    PaintTile(tile + glm::ivec2(0, 1));
}

bool RoadTool::Demolish(glm::ivec2 tile) {
    if (!m_Simulation.Roads().Remove(tile)) return false;
    glm::ivec3 minColumn(tile.x * TILE_SIZE, BUILD_GROUND_Y - 1, tile.y * TILE_SIZE);
    m_Editor.FillBox(minColumn, glm::ivec3(TILE_SIZE, 1, TILE_SIZE), Block::GRASS);
    PaintNeighbours(tile); // Their kerb comes back on this side
    return true;
}
