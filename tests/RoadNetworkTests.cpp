#include "Simulation/RoadNetwork.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <vector>

TEST(RoadNetworkTest, AddRemoveAndRevision) {
    RoadNetwork roads;
    uint32_t revision = roads.Revision();
    EXPECT_TRUE(roads.Add({ 3, -4 }));
    EXPECT_FALSE(roads.Add({ 3, -4 })); // Already road: no change
    EXPECT_TRUE(roads.IsRoad({ 3, -4 }));
    EXPECT_EQ(roads.Count(), 1u);
    EXPECT_EQ(roads.Revision(), revision + 1);

    EXPECT_TRUE(roads.Remove({ 3, -4 }));
    EXPECT_FALSE(roads.Remove({ 3, -4 }));
    EXPECT_FALSE(roads.IsRoad({ 3, -4 }));
    EXPECT_EQ(roads.Revision(), revision + 2);
}

TEST(RoadNetworkTest, KeysRoundTripNegativeTiles) {
    for (glm::ivec2 tile : { glm::ivec2(0, 0), glm::ivec2(-1, 5), glm::ivec2(123456, -654321), glm::ivec2(-7, -7) }) {
        EXPECT_EQ(RoadNetwork::KeyToTile(RoadNetwork::TileKey(tile)), tile);
    }
}

TEST(RoadNetworkTest, NewTilesAreUnreached) {
    RoadNetwork roads;
    roads.Add({ 0, 0 });
    ASSERT_NE(roads.Find({ 0, 0 }), nullptr);
    EXPECT_EQ(roads.Find({ 0, 0 })->distance, RoadTile::UNREACHED);
    EXPECT_EQ(roads.Find({ 1, 0 }), nullptr);
}

namespace {

// Every step of a path moves to a 4-neighbour
bool IsContinuous(const std::vector<glm::ivec2>& path) {
    for (size_t i = 1; i < path.size(); i++) {
        glm::ivec2 d = path[i] - path[i - 1];
        if (std::abs(d.x) + std::abs(d.y) != 1) return false;
    }
    return true;
}

} // namespace

TEST(RoadPathTest, SingleTile) {
    std::vector<glm::ivec2> path;
    MakeLPath({ 2, 2 }, { 2, 2 }, path);
    ASSERT_EQ(path.size(), 1u);
    EXPECT_EQ(path[0], glm::ivec2(2, 2));
}

TEST(RoadPathTest, StraightLines) {
    std::vector<glm::ivec2> path;
    MakeLPath({ 0, 0 }, { 5, 0 }, path);
    EXPECT_EQ(path.size(), 6u);
    EXPECT_TRUE(IsContinuous(path));
    MakeLPath({ 0, 0 }, { 0, -4 }, path);
    EXPECT_EQ(path.size(), 5u);
    EXPECT_EQ(path.back(), glm::ivec2(0, -4));
    EXPECT_TRUE(IsContinuous(path));
}

TEST(RoadPathTest, LShapeGoesAlongTheLongerAxisFirst) {
    std::vector<glm::ivec2> path;
    MakeLPath({ 0, 0 }, { 6, -2 }, path);
    EXPECT_EQ(path.size(), 9u); // 6 + 2 steps + the start
    EXPECT_EQ(path.front(), glm::ivec2(0, 0));
    EXPECT_EQ(path.back(), glm::ivec2(6, -2));
    EXPECT_EQ(path[6], glm::ivec2(6, 0)); // The corner: x first
    EXPECT_TRUE(IsContinuous(path));

    MakeLPath({ 0, 0 }, { -1, 7 }, path);
    EXPECT_EQ(path[7], glm::ivec2(0, 7)); // z first
    EXPECT_EQ(path.back(), glm::ivec2(-1, 7));
    EXPECT_TRUE(IsContinuous(path));
}

TEST(RoadPathTest, ClearsTheOutput) {
    std::vector<glm::ivec2> path = { { 9, 9 }, { 9, 10 } };
    MakeLPath({ 0, 0 }, { 1, 0 }, path);
    EXPECT_EQ(path.size(), 2u);
    EXPECT_EQ(path[0], glm::ivec2(0, 0));
}
