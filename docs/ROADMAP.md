# Voxel Anno roadmap

The plan for turning the engine into an Anno 1800-inspired city-builder (see `CLAUDE.md` for the coding rules). Each milestone is detailed when it starts. Milestones 1-3 and the scale-up (bigger grid, detailed buildings and people, bigger islands) are done; Milestone 4 is in progress.

## Roadmap

| # | Milestone | Result you can see |
|---|---|---|
| 1 (done) | Foundation & first building | Strategy camera, the island under the cursor is identified, place and demolish a building on a grid with a green/red preview, 10 Hz simulation tick, first tests |
| 2 (done) | Roads & warehouses | Paint roads; buildings only work when connected by road to a warehouse; each warehouse reaches a set distance along its roads; storage per island |
| 3 (done) | Housing & population | Farmer residences; needs (fish, work clothes) with supply %; population per island; houses upgrade or shrink |
| 4 (done) | Production chains | `ProductionComponent` (inputs, outputs, cycle time); fishery, sheep farm + pastures, framework knitter, lumberjack, sawmill; goods carried to warehouses; production UI |
| 5 (done) | Economy | Coins (resident taxes minus building upkeep), build costs in coins + materials, balance UI, game speed control |
| 6 | Ships & trade | Harbor; ships move across the ocean using the shore map's water mask; trade routes between islands; settling a second island |
| 7 | Higher tiers & content | Workers → Artisans → Engineers → Investors, with their needs and chains; NPC traders |
| 8 | Persistence & polish | Save/load (edited chunks + game objects), notifications, sound, balancing |

## Milestone 1, in detail (done)

### 1. Remove the "Project Strata" gameplay
- **Delete** `src/Gameplay/PlayerTools.h/.cpp` and `SceneVisuals` (beam, clump, flares).
- **Shaders:** remove the flare, laser, clump and beacon uniforms and code from `shaders/include/scene.glsl` and `shaders/render/shade.comp`.
- **Terrain:** remove artifact generation from `TerrainGenerator::GenerateChunk`, along with `Chunk::artifactIdx`, `Block::ARTIFACT` and the scanner.
- **Overlay:** remove mission text and the artifact count from `src/UI/DebugOverlay.cpp`. The minimap and engine metrics stay.
- **Debug edit tool:** a new `src/Gameplay/DebugEditTool.h/.cpp` keeps "dig a sphere" and "place block", using `WorldEditor` and `Raycast`. It's only active in debug mode.
- **Check:** the regression screenshot, with flares and the beacon gone, should otherwise match.

### 2. Cameras
- **`ICamera`** (`src/Gameplay/Camera.h`): `Position()`, `Front()`, `Up()`, `FocusPoint()`.
- **`StrategyCamera`** (`src/Gameplay/StrategyCamera.h/.cpp`):
  - orbits a ground target point, with yaw, pitch (clamped to about 35–80°) and distance (zoom);
  - WASD or screen-edge to pan, Q/E to rotate, mouse wheel to zoom, middle-drag to pan;
  - the cursor stays visible.

  The target's height follows the island ground (`SEA_LEVEL + ISLAND_HEIGHT`).
- **`FreeFlyCamera`:** today's `Player`, renamed. It keeps flight and collision.
- **Switching:** `Application` swaps cameras on F1 and builds `FrameParams` from the active one.
- **Streaming:** centered on the camera's `FocusPoint()`, not its position. Otherwise a zoomed-out camera would stream the wrong area.
- **Renderer:** the trace pass already handles a camera above the world slab (`tNear` entry in `shaders/render/trace.comp`), so no renderer change is needed. Confirm the strategy camera can go above y=128.

### 3. Mouse picking
- **`ScreenToWorldRay(cursor, camera, viewport)`** in `src/Gameplay/Picking.h/.cpp`. It uses the same inverse-projection math as `cameraRay` in `shaders/include/camera.glsl`, then calls the existing `Raycast` (`src/World/Raycast.cpp`).
- **Result:** the hovered voxel and its column. These drive placement and the debug overlay ("hovered column, island ID").

### 4. Simulation tick
- **`src/Simulation/GameClock.h/.cpp`:** a fixed-step accumulator, `int StepsToRun(float frameDelta)`, at 10 Hz with a cap of 5 steps per frame. It also exposes `Alpha()` for interpolating visuals.
- **`src/Simulation/Simulation.h/.cpp`:** a `FixedUpdate(float tickDt)` that owns all game systems. In M1 it only advances a tick counter, shown in the overlay.
- **`Application::RunFrame`:** calls `Simulation::FixedUpdate` for each step, before rendering.

### 5. Islands
- **`src/Simulation/IslandRegistry.h/.cpp`:** finds which island a column belongs to.
  - It flood-fills land over a coarse grid of 8×8-column cells.
  - "Land" means `TerrainGenerator::TerrainHeightAt >= SEA_LEVEL + ISLAND_HEIGHT`.

  This is deterministic and needs no voxel data, so it works for islands that aren't loaded yet.
- **IDs:** each island gets an ID from the order it's discovered, a list of its cells, and a bounding box. Results are cached. The flood fill has a size limit, so a huge landmass can't stall it.
- **Use:** placement checks that the whole footprint is on one island. Later, `IslandEconomy` (M3) hangs off the island ID.

### 6. Game objects and components
Follows `CLAUDE.md`: flat, pre-reserved component arrays and stable `uint32_t` IDs, with no pointers between components.
- **`src/Simulation/GameObjects.h/.cpp`:** `GameObjectRegistry`.
  - `Create()` / `Destroy(id)`.
  - Dense arrays of `BuildingComponent` (type, island ID, rotation) and `VoxelAnchorComponent` (origin voxel, footprint), indexed by ID with a free list.
  - Everything is reserved for 4096 objects at startup.
- **`src/Simulation/OccupancyGrid.h/.cpp`:** a hash map from column to object ID, to answer "is this column built on?" and "which building is here?".

### 7. Building definitions and placement
- **`src/Simulation/BuildingTypes.h`:** a `constexpr` table with ID, name, footprint (e.g. warehouse 4×4, farmer house 3×3), height, and how its look is built.
  - **Look:** for M1, procedural blocks: walls, roof, door.
  - **Block IDs:** new materials WOOD=40, PLANK=41, ROOF=42, STONE_WALL=43 in `src/World/BlockTypes.h`, with colors added in `getAlbedo` in `shaders/render/shade.comp`.
- **`src/Gameplay/BuildTool.h/.cpp`:** the build mode, run while a building is selected in the build menu.
  - **Footprint:** follows the hovered column, snapped to the grid; R rotates it.
  - **Validation:** every footprint column is flat grass at island height, on one island, not occupied, and not water.
  - **Left click:** stamps the voxels through `WorldEditor`, creates the `GameObject` and fills the occupancy grid.
  - **X / right click on a building:** demolishes it, restoring grass and dirt from `TerrainGenerator` for those columns, and removes the object.
- **Preview** in `shaders/render/shade.comp`: new uniforms `previewMin`, `previewMax` and `previewValid` tint hit voxels inside the box green or red, plus a translucent box outline. `FrameParams` gains a `BuildPreview` struct (this replaces `SceneVisuals`).

### 8. UI
- **`src/UI/BuildMenu.h/.cpp`:** an ImGui bar at the bottom with building buttons (Warehouse, Farmer House). It replaces the block hotbar in `src/UI/Hud.cpp`. The crosshair only shows in free-fly mode.
- **Overlay:** adds the camera mode, hovered column, island ID, building count and tick counter.

### 9. Tests (first version of the `CLAUDE.md` test setup)
- **New project `VoxelAnnoTests.vcxproj`** using Google Test from the NuGet package `Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn`. It compiles the pure-logic sources directly, which need no OpenGL: `GameClock`, `IslandRegistry`, `GameObjectRegistry`, `OccupancyGrid`, `TerrainGenerator`.
- **Tests:**
  - tick accumulation and the cap;
  - island IDs are stable across queries, and two islands separated by water get different IDs;
  - create/destroy reuses IDs;
  - placement validation rejects water, other buildings and footprints spanning two islands.
- **Docs:** `CLAUDE.md` test commands updated to the real paths (`x64\Debug\VoxelAnnoTests.exe`).

### Order of work
1, then 2+3 (you can look around and point at things), then 4, then 5+6, then 7, then 8, then 9. Build and play-test after each step. Commit per step.

## Milestone 2, in detail (done)

Roads are dragged as an L-shaped path and sit flush with the ground (the grass layer becomes a road block). A warehouse's range spreads along roads (30 road tiles); a building is connected when a road tile touching it is in range. Storage is per island and grows with each warehouse. No production, carts or costs yet (Milestones 4 and 5).

### 1. Items and island storage
- **`src/Simulation/ItemType.h`** (the path `CLAUDE.md` names):
  - `enum class ItemType : uint8_t` with the M3/M4 goods (Wood, Planks, Fish, Wool, WorkClothes, Bricks), plus `ITEM_COUNT`;
  - a `constexpr` name table.
- **`src/Economy/IslandEconomy.h/.cpp`:** the per-island manager that `CLAUDE.md` asks for.
  - `IslandStorage`: `std::array<int, ITEM_COUNT> amounts`, and `capacityPerItem`, which is the warehouse count × `WAREHOUSE_CAPACITY` (50).
  - `IslandEconomyManager`: maps an island ID to its `IslandStorage`, reserved up front.
  - `Add` / `Remove` return how much actually moved, clamped to the capacity and to what is stored.
  - The first warehouse on an island seeds starting goods (e.g. 30 Planks, 20 Wood). This is how Anno starts a settlement.
- **`Simulation`** owns the manager. `BuildTool::Place` and `Demolish` call `OnWarehouseAdded` and `OnWarehouseRemoved` on it.
- **UI:** `src/UI/IslandPanel.h/.cpp` is a small top-right window.
  - It shows the hovered (or last clicked) island's goods and capacity, and its warehouse count.
  - "No warehouse" appears for an island that hasn't been settled.

### 2. Road network data and road voxels
- **`src/Simulation/RoadNetwork.h/.cpp`:** a map from tile key to `RoadTile {uint16_t distance; GameObjectId warehouse;}`, reserved for 65536 tiles.
  - `Add(tile)`, `Remove(tile)`, `IsRoad(tile)`, and a revision counter. Anything derived from the network watches the counter to know when to rebuild.
  - **Look:**
    - Add writes `Block::ROAD_DIRT` (44, new) into the grass layer, `BUILD_GROUND_Y - 1`, for the tile's 4×4 columns, and clears grass tufts above.
    - Remove writes `GRASS` back.
    - Both go through the existing `WorldEditor::FillBox`.
- **Shading:** a road color goes into `getAlbedo` in `shaders/render/shade.comp`. Dirt brown with slight noise; ID 45 is reserved for cobble later.
- **Placement** (`src/Simulation/Placement.cpp`):
  - New `ValidateRoadTile(tile, world, islands, occupancy, roads)` with the same ground rules as buildings: grass at island height, only air above (2 voxels), on an island, no building there, not already road.
  - `ValidatePlacement` also rejects footprints with a road under them: new `PlacementError::Road`.
  - The occupancy grid stays building-only.

### 3. Tile overlay: per-tile colors drawn by the shade pass
Roads, the road-path preview and the warehouse range are all per-tile highlights. One general mechanism covers them all, and it is reused later for fertility, influence and so on.
- **`src/Rendering/TileOverlay.h/.cpp`:**
  - an R8UI texture of 256×256 tiles (1024×1024 columns), centered on the focus tile and re-centered when the focus moves far, as `ShoreMap` does;
  - a CPU byte buffer allocated once.
  - `Clear()`, `Set(tile, colorIndex)`, and `Upload()`, which uploads only when the buffer is dirty.
- **Shading** in `shade.comp`: when the hit is the top face of the ground layer (`mapPos.y == BUILD_GROUND_Y - 1`, normal up), look up the tile and tint by a small `const vec4` palette:
  - in range: soft blue;
  - out of range: grey;
  - preview add: green; preview invalid: red; preview remove: orange;
  - range of the selected warehouse: bright blue.
- **Renderer:** `VoxelRenderer` binds the texture (next free unit, 13) and passes `overlayOrigin`. `FrameParams` gets a pointer to the overlay.

### 4. Road tool: drag an L path
- **`src/Gameplay/RoadTool.h/.cpp`:** active when "Road [3]" is selected in `BuildMenu`. Hotkey 3; the selection is kept beside the `BuildTool` type.
  - **Path:** left press records the start tile. While dragging, the path is the start → end L, along the longer axis first (`MakeLPath(a, b, out)` into a reserved vector).
  - **Release:** adds every valid tile and skips invalid ones, as Anno does.
  - **Right-drag:** the same path in remove mode, deleting road tiles.
  - **Right click without a road selected** (`BuildTool`): over a road tile it demolishes that one tile; otherwise the current behavior stays.
- **Overlay:** each frame the tool writes the path into the `TileOverlay` preview layer, green or red per tile, or orange when removing.
- **Rebuilding:** the overlay is rebuilt only when the path, the network revision or the overlay origin changes. Hover alone does not trigger it.

### 5. Warehouse range and connection (simulation)
- **`src/Simulation/Logistics.h/.cpp`:** `LogisticsSystem::Rebuild(objects, roads)` runs in `Simulation::FixedUpdate` when the road revision or the building revision changed. Both are dirty counters.
  - **Search:** a multi-source breadth-first search over road tiles. It starts from the road tiles 4-adjacent to each warehouse's footprint, at distance 1, and stops at `WAREHOUSE_ROAD_RANGE` (30 tiles, 120 voxels).
  - **Result:** `RoadTile.distance` and the nearest `warehouse` are written into `RoadNetwork`.
  - **Memory:** the queue is a vector reserved at setup.
- **Per building:** a new `LogisticsComponent {GameObjectId warehouse; uint16_t roadDistance; bool connected;}` array in `GameObjectRegistry`.
  - A building is connected when any road tile touching its footprint has a distance at or below the range.
  - A warehouse is always "connected" to itself.
- **Overlay base layer:** reached road tiles are blue and unreached road tiles grey.
  - While a warehouse is selected or hovered, its own reach is bright blue.
  - While a warehouse is being placed, a preview of its reach is computed with the same search seeded from the preview footprint (`Logistics::PreviewReach`, using the reserved buffers).
- **`Simulation` gains:**
  - `BuildingsRevision()`, bumped by `BuildTool` on place and demolish;
  - `Roads()`;
  - `Logistics()`.

### 6. "No road" markers and UI
- **`WorldToScreen(camera, worldPos, windowSize)`** in `src/Gameplay/Picking.h/.cpp`: the inverse of `ScreenToWorldDirection`, using the same projection.
- **`src/UI/BuildingMarkers.h/.cpp`:** for each live, unconnected, non-warehouse building within view distance, draws a red road-sign style "!" badge above its roof with the foreground draw list. The loop runs over `GameObjectRegistry` slots and allocates nothing.
- **Overlay and tooltip:**
  - the building under the cursor shows "connected to warehouse #id, 7 road tiles" or "no road connection";
  - the `BuildMenu` status line adds "will not be connected" (yellow) when the preview footprint touches no road in range. Placement stays allowed, as in Anno.
- **F3 overlay:** road tile count and connected/total buildings.

### 7. Tests and docs
- **`tests/`:**
  - **Roads:**
    - `RoadNetworkTests`: add/remove, revision bumps, `MakeLPath` shapes (straight, L, single tile, negative directions);
    - `ValidateRoadTile` rejects water, buildings and existing roads.
  - **Logistics:**
    - `LogisticsTests` builds a synthetic road and object setup, with no voxels needed;
    - road distances;
    - range cutoff at exactly 30 and 31 tiles;
    - the nearest of two warehouses wins;
    - removing a road tile breaks the connection;
    - a building touching an in-range road is connected, and touching only an out-of-range road it is not.
  - **Economy:** `IslandEconomyTests` covers capacity growing with warehouses, `Add`/`Remove` clamping, and seeding only on the first warehouse.
  - **Placement:** a building on a road is rejected with `PlacementError::Road`.
- **Docs:**
  - `docs/ROADMAP.md`: mark M1 done, add the M2 detail;
  - `CLAUDE.md`: layout lines for `Economy/`, the new `Simulation/` and `UI/` files, and `TileOverlay`.

### Order of work
1 → 2 → 3 → 4 (roads can be painted and seen) → 5 → 6 (range and connection visible) → 7.
- Build and play-test after each step, and commit per step.
- Steps 1+2 and 5+6 can be reviewed together, as in M1.

## Milestone 3, in detail (done)

Residents move into houses that are connected to a warehouse and within a marketplace's road reach, and consume goods from island storage (a debug button adds goods until production arrives in Milestone 4). Each need's supply decides how many residents a house holds; a full, fully supplied Farmer house upgrades to a Worker house (2 planks), and a Worker house that stays at Farmer size falls back.

Residents also show up as **walkers**: one little voxel person (trousers, a shirt in the tier's color, a head) per 5 residents of an island steps out of a house next to a road and wanders the road network. They are visual only (`WalkerSystem`, outside the deterministic tick) and drawn straight into the GPU chunk data each frame (`WalkerRenderer`, `shaders/people/walkers.comp`), like the grass animation.

### 1. Data: goods, tiers, new buildings
- **`src/Simulation/ItemType.h`:** add `Sausages`, a Worker need, to the item list, and extend `STARTING_GOODS` to match.
- **`src/Economy/PopulationNeeds.h/.cpp`** (the name `CLAUDE.md` uses): a `constexpr` tier table.

  | Tier | Max residents | Needs (residents granted) | Consumption per resident per minute |
  |---|---|---|---|
  | Farmers | 10 | Market (2), Fish (4), Work Clothes (4) | Fish 0.050, Work Clothes 0.040 |
  | Workers | 20 | Market (2), Fish (6), Work Clothes (6), Sausages (6) | same, Sausages 0.030 |

  - A need is either a `Service` (Market) or a `Good`, with the residents it grants and its consumption.
  - Consumption is stored as **milli-goods per resident per minute**, so it stays integer.
- **`src/Simulation/BuildingTypes.h`:** add `Marketplace` (4×3 tiles) and `Worker House` (3×3, so it can replace a Farmer house in place).
  - New fields:
    - `role`: Storage, Residence or Market;
    - `tier`, for residences;
    - `lookStyle`.
  - The Worker house is **not** in the build menu; it only comes from upgrades.
- **`src/Simulation/BuildingLook.cpp`:** style variants.
  - The **Worker house** is taller, with a masonry ground floor and plank walls above.
  - The **marketplace** has low plank walls and a striped awning roof: new block `AWNING = 46`, with its color added in `shade.comp`.

### 2. Marketplace reach along roads
- **`src/Simulation/Logistics.cpp`:** turn the breadth-first search into a helper, `SpreadAlongRoads(sources, range, field)`, used twice:
  - warehouses, as now (`RoadTile.distance` and `warehouse`);
  - marketplaces: new `RoadTile.marketDistance` and `market` fields, with `MARKET_ROAD_RANGE = 20`.
- **What counts:** a marketplace only acts as a source when it is itself connected to a warehouse.
- **`LogisticsComponent`** gains `market`, `marketDistance` and `inMarketRange`. A house is "supplied" when it is connected to a warehouse **and** is in a market's range.
- **`TileOverlay`:**
  - new color `MARKET_REACH` (amber);
  - a hovered marketplace shows its reach;
  - placing a marketplace shows its preview reach (`PreviewReach` is generalized with a range parameter);
  - with a house or marketplace selected, roads in market range are tinted.

### 3. Population simulation
- **`ResidenceComponent`** (new flat array in `GameObjectRegistry`): `tier`, `residents`, a `growthTicks` counter, an `upgradeTicks` counter, `downgradeTicks`, and `needSupply[MAX_NEEDS]` (per mille).
- **`src/Economy/PopulationSystem.h/.cpp`:** `Update(objects, economy, ticks)` runs in `Simulation::FixedUpdate` after logistics.
  1. **Every second (10 ticks), consumption per island:**
     - Tiers go in order, Farmers first, so they get scarce goods first.
     - For each good the tier needs, the demand is the residents in supplied houses × the rate. It goes into a per-mille accumulator per (island, tier, good), owing at most 1 good.
     - Whole goods are removed from storage.
     - The cycle's supply fraction is what was delivered ÷ what was due. If nothing was due, it is 1 when the good is in stock and 0 otherwise, so new houses can start.
     - Island supply per (tier, good) moves a quarter of the way towards this cycle's fraction (exactly onto it when within 4‰), kept in `IslandStorage`.
  2. **Per house:**
     - `needSupply`: a Service need is 1000 if the house is in market range, otherwise 0. A Good need takes the island's tier supply if the house is supplied, otherwise 0.
     - The target is Σ ⌊grant × supply / 1000⌋.
     - Residents move ±1 toward the target every 20 ticks (2 s).
  3. **Upgrade:** a Farmer house with the maximum residents (10) and every need ≥ 900‰ for 100 ticks (10 s) upgrades, as long as the island has **2 Planks**.
     - The planks are removed.
     - The building type becomes Worker House, and the tier becomes Workers.
     - The id is pushed to a reserved `m_LookChanges` list.
  4. **Downgrade:** a Worker house with ≤ 10 residents for 300 ticks (30 s) becomes a Farmer house again, keeping ≤ 10 residents. Its id is pushed to `m_LookChanges` too.
  5. **Island totals:** population per tier, written to `IslandStorage.population[tier]`.
- **`Simulation`** owns `PopulationSystem` and exposes `ConsumeLookChanges()`.
- **Demolish:** removing a house simply removes its residents; the totals are recomputed every second.

### 4. Look refresh
- **`BuildTool::RefreshLook(id)`:** restamps the voxels with `BuildLook` for the object's current type through `WorldEditor::WriteBox`. It works because the footprint is the same and the height may differ: clear `max(oldHeight, newHeight)` first.
- **`Application`:** after the simulation steps each frame, it drains `ConsumeLookChanges()` and calls `RefreshLook`.

### 5. UI
- **`BuildMenu`:**
  - buttons: Warehouse [1], Farmer House [2], Marketplace [3], Road [4];
  - status line: "no marketplace in range" (yellow) when placing a house.
- **`IslandPanel`:**
  - population per tier (Farmers 40, Workers 20);
  - per-tier need supply bars (Fish 85%…);
  - goods (Sausages added);
  - a small **"+10 all goods (debug)"** button.
- **`src/UI/BuildingInfo.h/.cpp`:** a tooltip by the cursor while hovering a building with nothing selected.
  - **House:** tier, residents/max, each need with its %, and upgrade progress, or the reason it can't upgrade (needs, residents, planks).
  - **Marketplace:** houses in reach.
  - **Warehouse:** island goods summary.
- **`BuildingMarkers`:** keep the red "!" for no road, and add an **amber cart badge** for houses with a road but no marketplace in range.
- **F3 overlay:** total population and the number of upgrades this session.

### 6. Tests and docs
- **`tests/PopulationTests.cpp`:** objects, roads and economy built synthetically, with no voxels. Ticks run through `Simulation`-like fixtures.
  - A supplied house with goods in stock grows to 10 residents at +1 per 2 s.
  - Without a market it has 0 residents; without fish the target is Market and Work Clothes only.
  - Goods are used at the set rate, e.g. 100 residents use 5 fish per minute ±1.
  - Farmers are served before Workers when goods are scarce.
  - **Upgrade:** happens after 10 s fully supplied, uses 2 planks and emits a look change. Without planks there is no upgrade.
  - **Downgrade:** happens after 30 s at ≤ 10 residents.
- **`LogisticsTests`:**
  - market reach;
  - a marketplace that isn't connected to a warehouse supplies nothing;
  - the nearest marketplace wins.
- **`BuildingLookTests`:** new styles keep the volume = footprint × height; the Worker house has the same footprint as the Farmer house.
- **Docs:** `docs/ROADMAP.md` (M2 done, M3 detail) and the `CLAUDE.md` layout (`PopulationNeeds`, `PopulationSystem`, `BuildingInfo`).

### Order of work
1 → 2 → 3 → 4 → 5 → 6.
- Build and test throughout.
- One temporary demo patch for screenshots, which is removed afterwards.
- Play-test before committing, then one commit for M3.

## Scale-up, in detail (done)

Build tiles grow from 4 to 12 voxels (a farmer house is 36x36, roads are 12 wide, room for carts with horses), buildings become editable MagicaVoxel models, people about 10 voxels tall, and islands about 5x bigger (800-1150 voxels across).

### 1. Grid scale: `TILE_SIZE` 4 → 12
- **`src/Simulation/BuildingTypes.h`:**
  - set `TILE_SIZE = 12`;
  - add a `height` field (voxels) to `BuildingType`. It is the single source of truth for placement clearance, the preview box and the models.

  | Building | Height |
  |---|---|
  | Farmer house | 30 |
  | Worker house | 42 |
  | Warehouse | 40 |
  | Marketplace | 24 |

  - The world is 128 high and the ground sits at 46, so everything fits.
- **`BuildLook`:** stays as the procedural fallback when a model is missing, and is what the tests use. It scales its walls, door and windows to the bigger tiles and to `height`.
- **Everything else is in tiles and doesn't change:** footprints, occupancy, roads, ranges (warehouse 30 tiles = 360 voxels, market 20 tiles), logistics and population.
- **`ROAD_CLEARANCE`:** 3 → 12, so people and carts fit.
- **Roads:**
  - `RoadTool::Build` writes a 12×12 road with a 1-voxel `ROAD_EDGE` stone border (new block) and dirt with ruts inside;
  - removing a road restores grass.
- **Shade pass:** the tile overlay in `shade.comp` uses a floor division by 12 instead of `>> 2`, and tile borders come from `TILE_SIZE`, passed as a uniform.
- **Camera and screen code:**
  - `StrategyCamera`: zoom 60–1200 voxels, default 320, pan speed ×3;
  - `Picking`: `MAX_PICK_DISTANCE` 0.6 → 2.0 units;
  - `BuildingMarkers`: draw distance ×3.
- **Streaming:**
  - the CPU window grows from 16 → 20 chunks, so placement works at the screen edges when zoomed out. Empty upper layers cost little memory;
  - the default render distance becomes 24 (32 needs a second 768 MB chunk pool).

### 2. Bigger islands
- **`src/World/TerrainGenerator.cpp`:**
  - island noise frequency 0.0025 → 0.0005, with FBm octaves 3 → 5 so the coastlines keep their detail;
  - the coast blend band gets narrower in noise units (`n / 0.3` → `n / 0.06`), so beaches stay a sensible width instead of 5× wider.
- **Spawn search:** `FindSpawnColumn` probes 72 instead of 24, and its rings use step 48.
- **`IslandRegistry`:** `MAX_ISLAND_CELLS` 16384 → 65536.
- **`TestWorld`** (tests): radius 6 → 16 chunks, so water is still reachable from the spawn island.
- **Regression baseline:** the terrain changes on purpose, so the screenshot gets a **new** baseline after this step.

### 3. Building model pipeline
- **Palette convention:** palette index = block ID, so MagicaVoxel colors are the in-game colors.
  - New building material block IDs 60–95 in `src/World/BlockTypes.h`:
    - plaster white/cream, timber dark/light, thatch, roof tiles red/dark, stone light/dark, cobble;
    - window glass, window frame, door, shutters green/blue/red, chimney brick, flower box, fence, hay, crate, barrel, awning red/white.
  - Their colors live in a `const vec3` table indexed by ID in `shaders/render/shade.comp`, replacing the long if-chain for these IDs.
  - Windows get a warm glow at night.
- **`src/World/BuildingModel.h/.cpp`:** loads a `.vox` file into a dense `uint8_t` grid. It reuses the RIFF reading from `VoxModel.cpp` and converts MagicaVoxel's z-up to our y-up.
- **`BuildingModelLibrary`** loads `assets/buildings/<name>_<n>.vox` at startup.
  - Each type has 1–3 variants.
  - It checks the size against the footprint × 12 and `height`.
  - A missing or wrong file falls back to `BuildLook` with a console warning.
- **`BuildingComponent`** gains a `variant`, chosen at placement from the object ID, so neighbours differ.
- **`BuildTool::Place` and `RefreshLook`** stamp the model, rotated with the same quarter-turn mapping `BuildLook` uses. Upgrades swap to the Worker house model.

### 4. Generated detailed models
- **`tools/building_models/generate.py`** (Python, no dependencies) writes valid MagicaVoxel `.vox` files: `SIZE`, `XYZI`, and `RGBA` holding our palette.

  | Model | Size | Variants | Details |
  |---|---|---|---|
  | Farmer house | 36×36×30 | 3 | Timber frame with plaster infill, thatched roof, chimney, door with frame and step, shuttered windows, flower boxes, small fenced garden |
  | Worker house | 36×36×42 | 2 | Stone ground floor, framed plaster upper floor, tiled roof with dormer, two chimneys |
  | Warehouse | 48×48×40 | 1 | Stone and timber barn, big doors, hoist beam, crates and barrels outside |
  | Marketplace | 48×36×24 | 1 | Several striped stalls, goods crates, a well in the middle |

  - Variants change colors, chimney position and extensions.
- **Committed:** the script and the generated files in `assets/buildings/`.
- **`assets/buildings/README.md`:** the palette convention, the size rules (footprint × 12 by `height`, the front facing −y in MagicaVoxel), and how to regenerate.

### 5. Detailed people
- **`shaders/people/walkers.comp`:** walkers become ~10-voxel figures.
  - Two legs (4 tall, with a gap between them) and a torso 3 wide.
  - Arms at the sides that swing against the legs.
  - Head 2 tall, with a hat for Farmers (straw brim) and a cap for Workers.
  - A **3-frame walk cycle**, driven by the distance walked.
  - Figures are **rotated to the walking direction**.
  - Variety comes from skin, shirt and trousers variants: new person block IDs 51–59 with colors.
- **`WalkerFigure.w`** packs tier, direction, frame and variant.
- **Erasing** wipes the figure's bounding box (5×3×10), but only voxels that hold person IDs.
- **`WalkerSystem`:**
  - speed in voxels, about 5 voxels a second;
  - lanes at 3 and 8 columns into the 12-wide road, so people pass each other;
  - one walker per 5 residents, as now.

### 6. Tests, docs, baseline
- **Tests:**
  - `TestWorld` radius grows (step 2);
  - building look tests check `height`;
  - new `BuildingModelTests` load a generated `.vox` and check its size and that rotation keeps its blocks. The test project copies `assets/buildings` paths relative to the repo.
  - The existing tests stay in tiles, so most are unchanged.
- **Regression:** new baseline screenshot images. The old ones can't match because the terrain changed.
- **Docs:**
  - `CLAUDE.md`: layout lines for `BuildingModel`, `tools/`, `assets/buildings`, and the new scale;
  - `docs/ROADMAP.md`: add this scale-up between M3 and M4.

### Order of work
- Step 1, then step 2: play-test the scale and the islands, then commit.
- Steps 3–4: play-test the buildings, then commit.
- Step 5: play-test the people, then commit.
- The temporary demo patch screenshots each part and is removed afterwards.

## Milestone 4, in detail (done)

Producers with workforce, location rules and carts. Trees: one tree per clump of high tree noise (its peak), so a lumberjack can fell exactly one; felled trees are left out of generated chunks and grow back after 5 minutes unless a building or road covers the spot.

### The chains

| Building (tiles) | Workforce | Input → output | Cycle | Location rule |
|---|---|---|---|---|
| Fishery (3×3) | 5 Farmers | → Fish | 30 s | **Coast** (required): open sea within 2 tiles of the footprint |
| Lumberjack (3×3) | 5 Farmers | → Wood | 20 s | **Trees** within 6 tiles; productivity = standing trees ÷ 6. Fells the nearest tree each cycle; it regrows after 5 min |
| Sawmill (3×3) | 5 Farmers | Wood → Planks | 20 s | — |
| Sheep Farm (3×3) | 5 Farmers | → Wool | 30 s | **Pasture**: free grass tiles within 3 tiles; productivity = free tiles ÷ 24 |
| Framework Knitter (3×3) | 10 Farmers | Wool → Work Clothes | 30 s | — |
| Pig Farm (3×3) | 10 Workers | → Pigs (new good) | 60 s | **Pasture**, as for the sheep farm |
| Slaughterhouse (3×3) | 15 Workers | Pigs → Sausages | 30 s | — |

**Balance:**
- 1 fishery makes 2 fish a minute, which feeds 40 Farmers.
- One Farmer house gives 10 workforce, so a starting town of 6 houses can staff the first chains.

**Other rules:**
- A producer holds up to **4** of each input and **4** outputs.
- A **cart carries up to 4** goods and moves **0.6 tiles a second**, about 7 voxels a second.



### 1. Data and build menu
- **`ItemType`:** add `Pigs`.
- **`src/Economy/ProductionChains.h`:** a `constexpr` table, one row per producer:
  - workforce tier and amount;
  - up to 2 inputs and the output;
  - cycle ticks;
  - the location rule (`None`, `Coast`, `Trees`, `Pasture`) with its radius in tiles and its full-speed count.
- **`BuildingTypes`:**
  - new role `Producer` and a `chain` index;
  - the 7 types above, each with a `modelName` and a `height` (24–34).
- **`BuildMenu`** gets **tabs**:
  - Housing: Farmer House, Marketplace;
  - Production: the 7 producers;
  - Infrastructure: Warehouse, Road.
  - Number keys pick within the open tab.
  - **Tab** cycles tabs in strategy mode; it still frees the mouse in free-fly mode.

### 2. Producer models
`tools/building_models/generate.py` gains 7 models, 36×36, same palette rules, with the README updated:

| Model | Look |
|---|---|
| Fishery | Timber hut on a stone quay with a jetty sticking out at the back, nets on frames, fish crates, barrels |
| Lumberjack | Log cabin with a chopping block and axe, log piles, a saw horse |
| Sawmill | Open-sided shed with a saw bench and plank stacks, a log pile |
| Sheep farm | Barn with a fenced yard and hay racks. Sheep are white blocks: new `WOOL_WHITE` material |
| Framework knitter | Two-storey workshop with a loom visible through big windows and wool bales |
| Pig farm | Low sty with a mud pen, troughs and pigs: new `PIG_PINK` material |
| Slaughterhouse | Stone building with a smokehouse chimney and hanging sausages: new `SAUSAGE` material |

The new materials take block IDs 96–99.

### 3. Location rules — `src/Simulation/ProducerLocation.h/.cpp`
These are pure functions over the terrain noise, `IslandRegistry`, `OccupancyGrid`, `RoadNetwork` and `TreeRegistry`. No voxels are needed, so they are testable.
- **`Coast`:** some column within the radius has `TerrainHeightAt < SEA_LEVEL`. Otherwise placement fails with the new `PlacementError::NeedsCoast`.
- **`Pasture`:** counts free tiles in the radius: island ground, not road, not occupied.
- **`Trees`:** counts standing trees in the radius.
- **`LocationFactor`** is per mille. It is stored in `ProductionComponent` and recomputed when buildings, roads or trees change, so building a road through a pasture lowers its farm's output.
- **Placement preview:**
  - counted tiles (pasture or trees) and the coast ring are tinted in `TileOverlay` with a new color;
  - the build bar shows "Productivity 75% (18/24 pasture)" or "needs coast".

### 4. Trees — `src/Simulation/TreeRegistry.h/.cpp`
- **Tree positions:** `TerrainGenerator::IsTreeRoot(wx, wz)` holds the root test from `StampTrees`, and `ForEachTreeRoot(min, max, f)` builds on it. So trees are known without voxels.
- **The registry** stores only changed trees: a map from root to regrow tick, reserved up front.
  - Felling and regrowth push `TreeChange` events, which `Application` drains like look changes.
- **Visual consistency:**
  - the terrain generator skips felled trees. It reads a mutex-guarded copy of the felled set, so chunks generated later agree;
  - **loaded CPU chunks:** `WorldEditor::StampModel(root, model, erase)` removes a felled tree's voxels (only voxels still holding that model's IDs) or stamps a regrown tree back;
  - **GPU-only far chunks:** `ChunkStreamer::RegenerateChunk(key)` regenerates them in the background.

### 5. Production simulation — `src/Economy/Production.h/.cpp`
- **`ProductionComponent`** (flat array in `GameObjectRegistry`):
  - `progress`;
  - `inputs[2]` and `output`;
  - `locationFactor` and `productivity` (per mille);
  - `status`: Working, NoRoad, NoWorkforce, MissingInput, OutputFull, BadLocation;
  - the cart state (step 6).
- **`ProductionSystem::Update`** runs in `Simulation::FixedUpdate` after population.
  1. **Workforce, per island and tier:**
     - available = residents of that tier;
     - required = the workforce of every connected producer of that tier;
     - ratio = min(1000, available × 1000 ÷ required), stored in `IslandStorage.workforce[tier]` for the UI.
  2. **Per producer:**
     - productivity = ratio × locationFactor ÷ 1000;
     - it waits when output = 4 (OutputFull) or an input is 0 (MissingInput);
     - at the end of a cycle it takes the inputs and adds 1 output. A lumberjack also fells a tree.
- **Island panel:** workforce lines such as "Farmer jobs: 45 / 60 residents" (green), or red when short.

### 6. Carts: horse and wagon
- **State in `ProductionComponent`:** `Idle`, `ToWarehouse`, `Unloading`, `ToProducer`.
  - `path[31]` holds the road tiles.
  - `progress` per tile.
  - `cargo` holds an item and an amount.
- **Leaving:** the cart sets out when output ≥ 4, or when output > 0 and it has been idle 20 s.
- **Path:** downhill on the warehouse distance field (`RoadTile.distance`) from a road tile next to the producer. It needs no search, and its length is at most 30, the warehouse range.
- **At the warehouse:**
  - it unloads into island storage, and waits if storage is full;
  - it loads up to 4 of each input the producer is missing, then drives back along the same path.
- **Back home:** inputs go into the producer's buffer.
- **Cut road:** if the road is cut, the cart returns home with its cargo.
- **Drawing:** `ProductionSystem` exposes cart figures, interpolated between ticks with `GameClock::Alpha()`. `walkers.comp` draws a new **cart kind**:
  - a brown horse about 6 long and 6 tall, with a 4-frame leg cycle, a mane and a harness;
  - a 3-wide wagon with 4 wheels behind it;
  - a cargo crate colored by the good when loaded.
  - The figure's `w` gains a kind bit and the cargo item.
  - New block IDs for the horse and wagon wheels sit in the person and cart range, which becomes 47–59.
- **Lanes:** carts drive in the right-hand lane, and the erase box grows to cover them.

### 7. UI
- **Hover tooltip for a producer:**
  - status in words;
  - productivity, with workforce and location shown separately;
  - buffers;
  - the cart's state;
  - cycle progress.
- **Markers:** red (no road), grey "no workforce", orange "missing input".
- **F3:** producers working / total, and carts on the road.
- **The debug goods button moves into the F3 window:** the island panel stays clean now that goods are produced.

### 8. Tests and docs
- **`tests/ProductionTests.cpp`** (synthetic objects and roads):
  - a cycle takes `cycleTicks` at 100% and twice as long at 50% workforce;
  - a workforce shortage slows every producer of the tier;
  - the sawmill doesn't run without wood, and output stops at 4;
  - a cart trip takes the expected ticks per tile, delivers 4 to storage and brings inputs back;
  - a cut road sends the cart home;
  - Farmers' jobs are not filled by Workers.
- **`tests/ProducerLocationTests.cpp`** (`TestWorld`):
  - the fishery passes at the coast and gets NeedsCoast inland;
  - the pasture count drops when a road is built through it;
  - the lumberjack's tree count;
  - felling and regrowth events, with their timing;
  - the generator skips felled trees.
- **Model tests** cover the new producer models through the existing tests (every type has a model that fits, rotation).
- **Docs:** `docs/ROADMAP.md` (scale-up done, M4 detail) and the `CLAUDE.md` layout.

### Order of work
1–2, play-test, commit. Then 3–4, play-test, commit. Then 5–6, then 7–8, play-test, commit.
- Demo patches are used for screenshots and removed afterwards.
- `cdb` is available for any crash.

### Coastal fishery and chimney smoke (added during Milestone 4)

- **The fishery is a coastal building:** 3x4 tiles, the two back rows a plank dock on pilings over the water.
  - Placement counts columns: the land rows must not be open water, and at least half of them must be at island height. At least half of the dock columns must be open water.
  - The model reaches 6 voxels below ground; only its solid voxels are written there, and demolishing restores the generated terrain.
- **Fishing boats:** each fishery has a boat that leaves the dock, sails out up to 80 voxels (stopping before shallow water), fishes and comes back every 30 seconds. In step 5 this will follow the production cycle.
- **Chimney smoke:** voxel puffs rise from smoke emitters in the models (palette index 100), drift with the wind, grow and thin out. They appear while a building is in use.
- **Drawing:** people, smoke puffs and boats are all `Figure`s, drawn by `FigureRenderer` into the GPU chunk pools; erasing puts the sea back below sea level.
- **Shore map:** figures count as water, so a passing boat does not leave surf behind.

### Production (step 5, done)

- **Workforce:** `ProductionSystem` counts the jobs of the connected producers per island and tier against the residents of that tier. When there are fewer residents than jobs, every producer of that tier runs at the same reduced share. Workers do not take Farmer jobs, so upgrading every house leaves the Farmer producers without workers.
- **Location factors:** trees, pasture and coast are recomputed whenever buildings, roads or trees change.
- **Cycles:** progress grows by productivity (workforce share x location factor) while the inputs are there and the output buffer (4) has room; a cycle uses one of each input and makes one output. Lumberjacks fell the nearest tree each cycle.
- **Status** (`ProducerStatus`): working, no road, no workers, bad location, waiting for input, output full. It is shown in the tooltip and with grey and orange markers. Fishing boats sail and producers smoke only while working.
- **Next, step 6:** carts take the output to the warehouse and bring the inputs; until then, outputs stop at 4 and inputs only come from the buffer.

### Upgrades on request (after step 5)

- Houses no longer upgrade by themselves, so the player decides how many Farmers stay for the Farmer jobs.
- A full house with every need met for 10 s is **ready to upgrade**: a green arrow marker shows above it.
- With no build entry selected, left-clicking a building opens its panel on the right (Escape or clicking open ground closes it). For a ready house the panel has an **Upgrade** button, which costs 2 planks. The upgrade happens on the next tick, and only if the house is still ready and the planks are there.
- Downgrades stay automatic.

### Carts (step 6, done)

- **Simulation** (`ProductionSystem::UpdateCart`, state in `ProductionComponent`): one cart per producer, in integer thousandths of a road tile at 0.6 tiles a second.
  - **Leaving:** with a full load (4 goods); at once when an input ran out and the island has it; otherwise after waiting 20 s with something to carry.
  - **Route:** downhill on the warehouse road distance from the producer's best road tile (`FindCartPath`), at most 30 tiles.
  - **At the warehouse:** 2 s, then it unloads into island storage (waiting while storage is full) and loads up to 4 of each input the producer lacks.
  - **Cut road:** the cart turns around at the gap and brings its cargo home; with the way home cut it is home at once.
- **Drawing** (`src/Gameplay/Carts`): placed between ticks with `GameClock::Alpha`, in the right-hand lane, easing over to the new side after a turn. `figures.comp` draws the cart kind, about 23 voxels long: a trotting horse with mane, tail, collar, saddle pad and reins; shafts; a wagon on four turning wheels; a driver in a straw hat; one row of cargo per good, colored by the good. Block IDs 102-114 (`CART_FIRST`..`CART_LAST`) are figure blocks too.
- **F3:** `ProductionSystem::CartsOnRoad()` is ready for the overlay (step 7).

## Tile-aligned islands (done)

Islands are made of whole build tiles, like the blocky islands of Anno 1800, so buildings line up with the coast.

- **Tile kinds** (`TerrainGenerator::TileKindAt`):
  - **Land:** the island noise at the tile's middle is above the land threshold, smoothed by the four neighbours (no lone tiles or one-tile spikes, no one-tile notches). Flat grass at island height.
  - **Beach:** a non-land tile next to land, where a low-frequency coast noise is low. Sand slopes from one voxel under the grass into the water across the tile, following the square distance to the land, so corners stay square.
  - **Cliff:** the same where the coast noise is high (about a third of the coast). The grass ends in a rock edge (stone under the grass along that side) over water at least 6 deep.
  - **Sea:** past the beach, the floor slopes down with the distance to land (0.35 voxels per column) until it meets the deep noise floor; next to cliffs it is deep at once.
- **`TILE_SIZE`** moved to `src/World/WorldConstants.h`, since the terrain uses it too.
- **Caches:** the island noise and each tile's kind and distance to land are cached per generator (direct-mapped), so a column's height costs a few lookups.
- **Tests** (`tests/TerrainTests.cpp`): land tiles are flat everywhere and other tiles are below island height; coast tiles touch land and both kinds exist; beaches slope from above the sea to under it; no steps under the water away from cliffs; cliffs drop into deep water.

### UI, tests and docs (steps 7-8, done)

- **Producer tooltip and panel:** status, productivity split into workforce and location, buffers and cycle progress (from step 5), and now the cart: at home (and when it sets out), on its way with what it carries, unloading, waiting at a full warehouse, coming back with the inputs, or back after a cut road.
- **Markers:** red no road, grey no workers or bad location, amber missing input or no marketplace, green ready-to-upgrade arrow on houses.
- **F3:** producers working / total and carts on the road; the debug goods button moved here from the island panel (it adds to the island shown in the panel).
- **Tests:** production cycles and workforce, inputs and the output limit, carts (route, trip, waiting, fetching, cut road, full warehouse, placement), locations and trees, coastal placement, and the tile-aligned terrain.

## Milestone 5, in detail (done)

Coins belong to the player; planks come from the island a building stands on.

- **`Treasury`** (`src/Economy/Treasury.h/.cpp`, owned by `Simulation`, updated every tick after production):
  - **Taxes:** each house pays `TAX_PER_TEN_RESIDENTS` (Farmers 10, Workers 25 coins per minute per ten residents) times its residents, scaled by the average of its needs' supply. An unsupplied house pays nothing.
  - **Upkeep:** every building's `upkeep` from `BUILDING_COSTS` (warehouse 5, marketplace 15, producers 5-30 coins per minute; houses none).
  - **Integer math:** thousandths of a coin per minute; whole coins move once the remainder holds them.
  - **Debt:** the balance may go negative; then nothing can be built (`Check` fails), nothing else stops.
- **Build costs** (`BUILDING_COSTS`): coins and planks, e.g. farmer house 50c + 2 planks, sawmill 150c + 4 planks, warehouse 300c (no planks: it creates the storage). Roads are free. Starting coins: 3000.
  - The build preview turns red with "not enough coins" or "not enough planks on this island" (`PlacementError::NotEnoughCoins/NotEnoughPlanks`).
  - `BuildTool::Place` pays; `Demolish` refunds half of the coins and planks.
- **Game speed:** `GameClock::StepsToRun(frameDelta, speed)` with speed 0 (paused), 1, 2 or 4; the step cap grows with the speed. Walkers, smoke and boats follow the game speed; day and night stay on real time. Keys: P pauses, + and - step the speed.
- **UI:** the top bar shows coins and the net income per minute (taxes and upkeep on hover) and the speed buttons; build buttons show the cost; building tooltips show taxes or upkeep and the demolish refund; F3 shows coins, taxes and upkeep.
- **Tests:** `TreasuryTests` (tax at full and half supply, empty houses, upkeep into debt, `Check`, pay and refund) and `GameClockTests` (speed scales the steps, pause runs none, the cap grows).

## Milestone 6, in detail

A **Harbor** building, **ships** built there that sail the open sea, direct **move orders** and **trade routes**, and **settling a second island** with materials brought by ship. Coins stay the player's; goods stay on their island until a ship moves them. Done in steps, each play-tested and committed.

### 1. Harbor building
- New type `Harbor`: 4x5 tiles (3 land rows, 2 dock rows), `belowGround` 6, role `Storage` (so it is a warehouse: road reach, capacity), category Infrastructure, model `harbor`; cost 400 coins + 6 planks, upkeep 10.
- Code that meant "warehouse" now means "storage building" (`OnWarehouseAdded/Removed`, the reach preview).
- Placement reuses the coastal dock rules of the fishery (`dockRows`, `DockNotOverWater`).
- Model (`tools/building_models/generate.py harbor_1`): a stone-and-timber harbor office, a storage shed, a wide pier on pilings with bollards, crates and a crane, and a boat berth marker (101) at the pier's end.

### 2. Ships in the simulation (`src/Simulation/Ships.h/.cpp`)
- `ShipSystem` in `Simulation`: up to 32 ships with generation IDs; position in thousandths of a tile, a waypoint path reserved up front, state Idle / Sailing / Docked, 2 cargo slots of 50, an optional trade route; 1.5 tiles a second.
- Navigation on the tile grid from `TerrainGenerator::TileKindAt` (Sea and Cliff tiles are open water), so routes cross the whole map without loaded chunks: A* with 8 neighbours (no corner cutting), on a fixed 512x512-tile window allocated once; run when an order is given or a route leg starts.
- Docking at a harbor's berth (marker position, as the fishing boats find theirs). "Build ship" in the harbor panel: 500 coins + 20 planks from the island's storage.

### 3. Drawing, selection, orders, cargo
- Ships are voxel objects (like the fishing boats): a trade ship model ~25 long with two masts and sails, deck cargo colored by the goods, turning smoothly and bobbing, casting shadows.
- Left click near a ship selects it (a ring and a ship panel); right click on the sea sends it there, on a harbor to its berth.
- Ship panel: state, cargo, route; while docked, +10/-10 buttons move goods between the ship and that island's storage.

- Steps 2 and 3 were done together (a ship has to be seen to be play-tested). The berth is two tiles past the pier's end, clear of it; a new ship faces out to sea.

### 4. Trade routes
- Up to 16 routes of up to 4 stops; each stop is a harbor and, per good, Load / Unload / nothing.
- At a stop the ship waits 3 s, unloads the goods marked Unload (waiting while storage is full), loads the goods marked Load until full or the storage is empty, and sails on. A stop whose harbor is gone is skipped.
- Route panel (`src/UI/TradeRoutes.h/.cpp`): routes, stops with a harbor dropdown, per-good toggles, "assign selected ship".

- Done: a right-click order takes a ship off its route; the window opens from the top bar ("Routes").

### 5. Settling a second island
- The first storage building ever is free of the rule and gets the starting goods.
- On any other island without storage, the first storage building needs one of your ships anchored or docked within 4 tiles, and its planks come from that ship (`NeedsShip`); no starting goods there.

### 6. UI polish, tests, docs
- Markers for ships waiting at a full harbor; the harbor panel lists docked ships; F3 counts ships; the unsettled-island hint names the ship rule.
- Tests per step (harbor placement and storage, paths that never cross land, docking, cargo limits, route loops, settling); `CLAUDE.md` and this roadmap.
