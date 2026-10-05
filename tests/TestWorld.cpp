#include "TestWorld.h"

#include "World/Chunk.h"

TestWorld& TestWorld::Get() {
    static TestWorld instance;
    return instance;
}

TestWorld::TestWorld() {
    spawnColumn = terrain.FindSpawnColumn(glm::ivec2(640, 640)); // Same search start as the game
    LoadAround(spawnColumn, RADIUS_CHUNKS);
}

void TestWorld::LoadAround(glm::ivec2 column, int radiusChunks) {
    glm::ivec2 center(column.x >> 5, column.y >> 5);
    for (int cz = center.y - radiusChunks; cz <= center.y + radiusChunks; cz++) {
        for (int cx = center.x - radiusChunks; cx <= center.x + radiusChunks; cx++) {
            if (world.FindChunk(cx, 1, cz)) continue;
            Chunk* chunk = world.GetOrCreateChunk(cx, 1, cz);
            terrain.GenerateChunk(cx, 1, cz, chunk->data);
        }
    }
}
