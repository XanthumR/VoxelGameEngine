#include "TestWorld.h"

#include "World/Chunk.h"

TestWorld& TestWorld::Get() {
    static TestWorld instance;
    return instance;
}

TestWorld::TestWorld() {
    spawnColumn = terrain.FindSpawnColumn(glm::ivec2(640, 640)); // Same search start as the game
    glm::ivec2 spawnChunk(spawnColumn.x >> 5, spawnColumn.y >> 5);
    for (int cz = spawnChunk.y - RADIUS_CHUNKS; cz <= spawnChunk.y + RADIUS_CHUNKS; cz++) {
        for (int cx = spawnChunk.x - RADIUS_CHUNKS; cx <= spawnChunk.x + RADIUS_CHUNKS; cx++) {
            Chunk* chunk = world.GetOrCreateChunk(cx, 1, cz);
            terrain.GenerateChunk(cx, 1, cz, chunk->data);
        }
    }
}
