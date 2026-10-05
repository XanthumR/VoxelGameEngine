# Voxel Anno roadmap

The plan for turning the engine into an Anno 1800-inspired city-builder (see `CLAUDE.md` for the coding rules). Milestone 1 is detailed; later milestones are refined when they start.

## Roadmap

| # | Milestone | Result you can see |
|---|---|---|
| **1** | **Foundation & first building** | Strategy camera, the island under the cursor is identified, place and demolish a building on a grid with a green/red preview, 10 Hz simulation tick, first tests |
| 2 | Roads & warehouses | Paint roads; buildings only work when connected by road to a warehouse; each warehouse has a catchment radius; storage per island |
| 3 | Housing & population | Farmer residences; needs (fish, work clothes) with supply %; population per island; houses upgrade or shrink |
| 4 | Production chains | `ProductionComponent` (inputs, outputs, cycle time); fishery, sheep farm + pastures, framework knitter, lumberjack, sawmill; goods carried to warehouses; production UI |
| 5 | Economy | Coins (resident taxes minus building upkeep), build costs in coins + materials, balance UI, game speed control |
| 6 | Ships & trade | Harbor; ships move across the ocean using the shore map's water mask; trade routes between islands; settling a second island |
| 7 | Higher tiers & content | Workers → Artisans → Engineers → Investors, with their needs and chains; NPC traders |
| 8 | Persistence & polish | Save/load (edited chunks + game objects), notifications, sound, balancing |

## Milestone 1, in detail

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
