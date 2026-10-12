#include "Simulation/BuildingLook.h"
#include "Simulation/BuildingTypes.h"
#include "Simulation/SaveGame.h"
#include "Simulation/Simulation.h"
#include "TestWorld.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

namespace {

// Places a building like BuildTool::Place (without the look), at the first valid tile in rings
// round the spawn
GameObjectId PlaceNear(Simulation& sim, uint16_t type, int startRing = 0) {
    TestWorld& test = TestWorld::Get();
    PlacementContext context = sim.MakePlacementContext(test.world);
    glm::ivec2 spawn(ColumnToTile(test.spawnColumn.x), ColumnToTile(test.spawnColumn.y));
    for (int ring = startRing; ring < 20; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                glm::ivec2 tile = spawn + glm::ivec2(dx, dz);
                PlacementCheck check = ValidatePlacement(context, type, 0, tile);
                if (check.error != PlacementError::None) continue;
                GameObjectId id = sim.Objects().Create();
                BuildingComponent& building = sim.Objects().Building(id);
                building.type = type;
                building.island = check.island;
                glm::ivec2 tiles = FootprintTiles(BUILDING_TYPES[type], 0);
                sim.Objects().Anchor(id).origin = glm::ivec3(tile.x * TILE_SIZE, BUILD_GROUND_Y, tile.y * TILE_SIZE);
                sim.Objects().Anchor(id).footprint = tiles * TILE_SIZE;
                sim.Occupancy().Occupy(tile, tiles, id);
                if (BUILDING_TYPES[type].role == BuildingRole::Storage) sim.Economy().OnWarehouseAdded(check.island);
                sim.MarkBuildingsChanged();
                return id;
            }
        }
    }
    return INVALID_GAME_OBJECT;
}

// A small settlement: a warehouse, a road round it, houses, a marketplace and producers, a felled
// tree, coins spent
void BuildSettlement(Simulation& sim) {
    TestWorld& test = TestWorld::Get();
    PlacementContext context = sim.MakePlacementContext(test.world);
    GameObjectId warehouse = PlaceNear(sim, BUILDING_WAREHOUSE);
    ASSERT_NE(warehouse, INVALID_GAME_OBJECT);
    // A ring of road round the warehouse, a few tiles out
    glm::ivec2 corner(ColumnToTile(sim.Objects().Anchor(warehouse).origin.x), ColumnToTile(sim.Objects().Anchor(warehouse).origin.z));
    for (int i = -1; i <= 4; i++) {
        for (glm::ivec2 tile : { corner + glm::ivec2(i, -1), corner + glm::ivec2(i, 4), corner + glm::ivec2(-1, i), corner + glm::ivec2(4, i) }) {
            if (ValidateRoadTile(context, tile).error == PlacementError::None) sim.Roads().Add(tile);
        }
    }
    for (uint16_t type : { BUILDING_FARMER_HOUSE, BUILDING_FARMER_HOUSE, BUILDING_FARMER_HOUSE, BUILDING_MARKETPLACE, BUILDING_LUMBERJACK,
             BUILDING_SAWMILL }) {
        EXPECT_NE(PlaceNear(sim, type, 3), INVALID_GAME_OBJECT);
    }
    sim.Trees().Fell(test.spawnColumn + glm::ivec2(3, 5), sim.TickCount());
    sim.Coins().SetCoins(1234);
}

std::vector<uint8_t> SaveOf(const Simulation& sim) {
    SaveWriter out;
    sim.Save(out);
    return out.Data();
}

} // namespace

TEST(SaveGameTest, LoadingGivesBackTheSameStateAndItRunsOnTheSame) {
    TestWorld& test = TestWorld::Get();
    auto original = std::make_unique<Simulation>(test.terrain);
    BuildSettlement(*original);
    for (int i = 0; i < 600; i++) original->FixedUpdate(0.1f); // A minute: residents, production, carts
    std::vector<uint8_t> saved = SaveOf(*original);

    // Loaded into a fresh one, it saves to the same bytes
    auto loaded = std::make_unique<Simulation>(test.terrain);
    SaveReader in(saved);
    ASSERT_TRUE(loaded->Load(in));
    EXPECT_EQ(SaveOf(*loaded), saved);
    EXPECT_EQ(loaded->Objects().AliveCount(), original->Objects().AliveCount());
    EXPECT_EQ(loaded->Roads().Count(), original->Roads().Count());
    EXPECT_EQ(loaded->Coins().Coins(), original->Coins().Coins());
    EXPECT_EQ(loaded->Occupancy().OccupiedTiles(), original->Occupancy().OccupiedTiles());

    // ...and both play on alike: nothing that matters was left out
    for (int i = 0; i < 1800; i++) {
        original->FixedUpdate(0.1f);
        loaded->FixedUpdate(0.1f);
    }
    EXPECT_EQ(SaveOf(*loaded), SaveOf(*original));
}

TEST(SaveGameTest, DamagedDataFailsToLoad) {
    TestWorld& test = TestWorld::Get();
    auto original = std::make_unique<Simulation>(test.terrain);
    BuildSettlement(*original);
    std::vector<uint8_t> saved = SaveOf(*original);

    auto loaded = std::make_unique<Simulation>(test.terrain);
    SaveReader cut(std::vector<uint8_t>(saved.begin(), saved.begin() + (long)saved.size() / 2));
    EXPECT_FALSE(loaded->Load(cut));
    std::vector<uint8_t> longer = saved;
    longer.push_back(0);
    SaveReader extra(longer);
    EXPECT_TRUE(loaded->Load(extra));
    EXPECT_FALSE(extra.AtEnd()); // The caller sees the extra byte
}

TEST(SaveGameTest, ListsTheSavesNewestFirst) {
    std::filesystem::path directory = std::filesystem::temp_directory_path() / "voxtaria_save_test";
    std::filesystem::remove_all(directory);
    for (int i = 0; i < 2; i++) {
        SaveWriter out;
        out.Write(SAVE_MAGIC);
        out.Write(SAVE_VERSION);
        SaveSummary summary;
        summary.savedAt = 1000 + i;
        summary.residents = 10 * i;
        out.Write(summary);
        ASSERT_TRUE(out.WriteFile((directory / ("game" + std::to_string(i) + ".sav")).string()));
    }
    SaveWriter old; // From another version
    old.Write(SAVE_MAGIC);
    old.Write(SAVE_VERSION - 1);
    ASSERT_TRUE(old.WriteFile((directory / "old.sav").string()));

    std::vector<SaveFileInfo> saves = ListSaves(directory.string());
    ASSERT_EQ(saves.size(), 3u);
    EXPECT_EQ(saves[0].name, "game1");
    EXPECT_EQ(saves[0].summary.residents, 10);
    EXPECT_TRUE(saves[0].loadable);
    EXPECT_EQ(saves[1].name, "game0");
    EXPECT_TRUE(saves[1].loadable);
    EXPECT_EQ(saves[2].name, "old");
    EXPECT_FALSE(saves[2].loadable);
    std::filesystem::remove_all(directory);
}
