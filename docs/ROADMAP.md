# Voxel Anno roadmap

The plan for turning the engine into an Anno 1800-inspired city-builder (see `CLAUDE.md` for the coding rules). Each milestone is detailed when it starts. Milestones 1 and 2 are done; Milestone 3 is in progress.

## Roadmap

| # | Milestone | Result you can see |
|---|---|---|
| 1 (done) | Foundation & first building | Strategy camera, the island under the cursor is identified, place and demolish a building on a grid with a green/red preview, 10 Hz simulation tick, first tests |
| 2 (done) | Roads & warehouses | Paint roads; buildings only work when connected by road to a warehouse; each warehouse reaches a set distance along its roads; storage per island |
| **3** | **Housing & population** | Farmer residences; needs (fish, work clothes) with supply %; population per island; houses upgrade or shrink |
| 4 | Production chains | `ProductionComponent` (inputs, outputs, cycle time); fishery, sheep farm + pastures, framework knitter, lumberjack, sawmill; goods carried to warehouses; production UI |
| 5 | Economy | Coins (resident taxes minus building upkeep), build costs in coins + materials, balance UI, game speed control |
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

## Milestone 3, in detail

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
