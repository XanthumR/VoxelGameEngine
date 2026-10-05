#include "Simulation/RoadNetwork.h"

#include <cstdlib>

RoadNetwork::RoadNetwork() {
    m_Tiles.reserve(RESERVED_TILES);
}

bool RoadNetwork::Add(glm::ivec2 tile) {
    if (!m_Tiles.emplace(TileKey(tile), RoadTile()).second) return false;
    m_Revision++;
    return true;
}

bool RoadNetwork::Remove(glm::ivec2 tile) {
    if (m_Tiles.erase(TileKey(tile)) == 0) return false;
    m_Revision++;
    return true;
}

RoadTile* RoadNetwork::Find(glm::ivec2 tile) {
    auto it = m_Tiles.find(TileKey(tile));
    return it == m_Tiles.end() ? nullptr : &it->second;
}

const RoadTile* RoadNetwork::Find(glm::ivec2 tile) const {
    auto it = m_Tiles.find(TileKey(tile));
    return it == m_Tiles.end() ? nullptr : &it->second;
}

void MakeLPath(glm::ivec2 a, glm::ivec2 b, std::vector<glm::ivec2>& out) {
    out.clear();
    glm::ivec2 delta = b - a;
    glm::ivec2 step(delta.x > 0 ? 1 : (delta.x < 0 ? -1 : 0), delta.y > 0 ? 1 : (delta.y < 0 ? -1 : 0));
    bool xFirst = std::abs(delta.x) >= std::abs(delta.y);

    glm::ivec2 tile = a;
    out.push_back(tile);
    for (int leg = 0; leg < 2; leg++) {
        int axis = (leg == 0) == xFirst ? 0 : 1;
        while (tile[axis] != b[axis]) {
            tile[axis] += step[axis];
            out.push_back(tile);
        }
    }
}
