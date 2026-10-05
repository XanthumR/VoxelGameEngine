#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;
class Player;
class VoxelWorld;
class WorldEditor;

// Debug terrain editing from the free-fly camera: left click digs a small sphere where the
// crosshair points, right click places the selected block against the face, 1-5 select a block.
// Active only while the mouse is captured.
class DebugEditTool {
public:
    static constexpr float REACH_DISTANCE = 10.0f; // World units
    static constexpr int DIG_RADIUS = 2;            // Voxels

    DebugEditTool(VoxelWorld& world, WorldEditor& editor);

    void HandleBlockSelectKeys(GLFWwindow* window);
    void HandleMouse(GLFWwindow* window, const Player& player);

    // Green when pointing at a block within reach, white otherwise
    glm::vec3 CrosshairColor(const Player& player) const;

    int& SelectedBlock() { return m_SelectedBlock; } // 1 = Grass, 2 = Dirt, 3 = Stone, 4 = Sand, 5 = Plant

private:
    VoxelWorld& m_World;
    WorldEditor& m_Editor;
    int m_SelectedBlock = 1;
    bool m_RightWasPressed = false;
};
