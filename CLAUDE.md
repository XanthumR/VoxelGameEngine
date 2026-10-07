# Claude Project Instructions: Voxel Anno (MSVC & Custom C++)

Project-specific guidelines for a voxel-based city-builder and logistics simulation game written in C++ (MSVC compiler), utilizing a custom object/component architecture.

## 🛠️ Build & Run Commands

### MSVC Development & Build (Developer PowerShell / Command Prompt)
* **Build System:** Visual Studio solution built with MSBuild (no CMake). GLFW location is the `GlfwDir` property in `VoxelGameEngine.vcxproj` (override with `/p:GlfwDir=...`).
* **Build Project (Debug):** `msbuild VoxelGameEngine.sln /p:Configuration=Debug /p:Platform=x64 /m`
* **Build Project (Release):** `msbuild VoxelGameEngine.sln /p:Configuration=Release /p:Platform=x64 /m`
* **Run Game Client:** `.\x64\Debug\VoxelGameEngine.exe` (run from the project root so `shaders\` and `assets\` are found)
* **Launch options:** `--render-distance N`, `--render-scale F`, and for reproducible runs `--fixed-time T`, `--camera X Y Z YAW PITCH`, `--screenshot FILE SECONDS` (see `src/Core/LaunchOptions.h`).
* **Regression check:** render the same view before and after a change with those three options and compare the PPMs pixel by pixel; the scene is deterministic (only the FPS text in the overlay differs between runs).

### Testing & Profiling
* **Test project:** `tests/VoxelAnnoTests.vcxproj` (Google Test from NuGet, built with the solution). It compiles the pure-logic sources (`src/Simulation/`, terrain, `VoxelWorld`) at `/W4 /WX`; no OpenGL or window.
* **Restore Google Test (once, or after a clean clone):** `msbuild VoxelGameEngine.sln -t:restore -p:RestorePackagesConfig=true` (Visual Studio restores it automatically)
* **Run All Tests:** `.\x64\Debug\VoxelAnnoTests.exe`
* **Run Specific Simulation Test:** `.\x64\Debug\VoxelAnnoTests.exe --gtest_filter=PlacementTest.*`
* **Format Code:** `clang-format -i -style=file src/**/*.cpp src/**/*.h`

---

## 🏗️ Architecture & Tech Stack

### Core Tech
* **Compiler & Standard:** MSVC (Visual Studio 2022), targeting C++20 (`/std:c++20`).
* **Architecture:** Custom Object-Component system. Game objects (`GameObject`) hold raw vectors of pre-allocated, flat component arrays to maintain cache locality without third-party ECS overhead.
* **Threading:** Win32 Thread Pool / `std::jthread` for asynchronous voxel meshing and off-loop trade route updates.

### Project Layout (what does which job)
* `src/main.cpp` – entry point; builds `Application` from the launch options.
* `src/Core/` – `Application` (window, OpenGL/ImGui setup, spawn, the frame loop in order, hotkeys), `LaunchOptions`, `Screenshot`, `SafeQueue`.
* `src/World/` – CPU-side world: `WorldConstants` (chunk size, sea level, `TILE_SIZE` build grid), `BlockTypes` (block IDs), `Chunk`, `VoxelWorld` (CPU chunk store), `TerrainGenerator` (islands made of whole build tiles: land, beach and cliff coast tiles, sea floor; caves, trees, spawn search), `ChunkStreamer` (worker threads + streaming windows), `WorldEditor` (dig/place, box writes, model stamps), `FelledTrees` (felled tree set shared with the chunk workers), `Raycast`, `VoxModel` (.vox loader for trees), `BuildingModel` (.vox building model as a dense grid), `GrassTufts`.
* `src/Rendering/` – GPU side: `GpuChunkCache` (pool textures, page table, brick masks), `VoxelRenderer` (trace/shadow/shade passes), `RenderTargets`, `RenderSettings`, `SkyLighting` (day-night), `OceanSimulation` (FFT waves), `ShoreMap` (coast distance field), `GrassAnimator`, `ShaderLoader` (supports `#include`), `BuildPreview` (placement ghost: the model half see-through, tinted green or red, ray-marched by the shade pass from a 3D texture; a box around the selected building), `TileOverlay` (per build tile ground colors: road range, road previews, warehouse reach), `FigureRenderer` (draws chimney smoke puffs into the GPU chunk pools each frame, `shaders/people/figures.comp`), `VoxelObject` (moving things with their own voxel model and a free position and rotation, like Teardown: the shade pass ray-marches each model inside its oriented box; models in one atlas, `VoxelRenderer::AddObjectModel`; each frame objects are sorted into 16x16-pixel screen tiles so a pixel only tests the few near it; they cast sun and moon shadows through a world shadow grid of 16x16-column cells; objects with a waterline float: `shaders/render/float.comp` sets their height, pitch and roll from the FFT ocean under them each frame). Walkers, carts and fishing boats are voxel objects.
* `src/Gameplay/` – `Camera` (`ICamera` interface), `StrategyCamera` (default Anno-style orbit camera), `FreeFlyCamera` (debug drone with collision, F1), `Picking` (cursor ray to hovered voxel), `BuildTool` (build selection, place/demolish buildings with preview, strategy camera), `RoadTool` (drag L-shaped roads, right-drag removes), `DebugEditTool` (dig/place in free-fly), `Walkers` (`WalkerSystem`: visual residents wandering the roads, one per 5 residents; ~11-voxel people with a walk cycle, turning smoothly), `Smoke` (chimney smoke puffs from model emitters), `FishingBoats` (each fishery's boat, a voxel object, sails out at a new angle each trip, turns and bobs), `Carts` (places each producer's cart between ticks, in the right-hand lane, turning smoothly through corners and on the spot at the warehouse), `FigureModels` (voxel object models of people and carts, one per look and animation frame), `ShipControl` (selecting ships, move orders by right click, the trade ship model and its drawing), `Figure` (a smoke puff for `FigureRenderer`).
* `src/Simulation/` – deterministic game logic: `GameClock` (fixed 10 Hz step accumulator, game speed 0/1/2/4), `Simulation` (owns game systems, `FixedUpdate` per step), `IslandRegistry` (island ID per column, flood fill over 8x8-column cells), `GameObjects` (`GameObjectRegistry`: slot+generation IDs, flat component arrays), `OccupancyGrid` (build tile -> game object), `BuildingTypes` (building table on the `TILE_SIZE` build grid), `BuildingLook` (procedural fallback look of a building), `BuildingModels` (`BuildingModelLibrary`: the .vox variants of each building type, rotated and stamped), `Placement` (`ValidatePlacement`, `ValidateRoadTile`), `ItemType` (goods), `RoadNetwork` (road tiles + `MakeLPath`), `Logistics` (warehouse and marketplace reach along roads, building connections), `TreeRegistry` (felled trees, regrowth, tree change events), `ProducerLocation` (coast, trees and pasture rules for producers), `Ships` (`ShipSystem`: the player's ships, A* over open-water build tiles, docking at harbor berths, cargo to and from island storage, trade routes that ships sail on their own, settling new islands: the first storage building there needs a ship nearby carrying its planks). `BuildingTypes` also holds each building's role (storage, residence, market), tier and look style; the warehouse and the coastal harbor are both storage buildings.
* `src/Economy/` – `IslandEconomy` (`IslandEconomyManager`: per-island storage, capacity from warehouses, starting goods, population and need supply), `PopulationNeeds` (tier table: needs, residents granted, consumption), `PopulationSystem` (consumption per island and tier, residents moving in and out, houses ready to upgrade, upgrades the player requests, automatic downgrades), `ProductionChains` (producer recipes: workforce, inputs, output, cycle, location rule), `Production` (`ProductionSystem`: workforce per island and tier, location factors, production cycles, lumberjacks felling trees, the producers' carts between producer and warehouse), `Treasury` (the player's coins: taxes scaled by needs met, upkeep, build costs and half refunds; the cost table `BUILDING_COSTS`).
* `src/UI/` – `DebugOverlay` (F3 window + minimap, simulation counters, debug goods button), `Hud` (free-fly crosshair + block hotbar), `BuildMenu` (strategy build bar), `IslandPanel` (island population, need supply, workforce, storage), `TopBar` (coins, income per minute, game speed buttons), `ShipPanel` (the selected ship: marker, state, cargo, route, loading while docked), `TradeRoutes` (the route window: stops, per-good Load/Unload, assigning ships), `BuildingMarkers` (problem badges: no road, no marketplace, no workers, missing input; ready-to-upgrade arrows), `BuildingInfo` (hover tooltip and clicked-building panel: house residents, needs and upgrade with its button; producer status, productivity, buffers, cart).
* `src/ThirdParty/` – `FastNoiseLite.h`, `glad.c`.
* `assets/buildings/` – building models (MagicaVoxel, palette index = block ID 60-99; see its `README.md`); `tools/building_models/generate.py` generates the first versions.
* `tests/` – `VoxelAnnoTests` (gtest): clock, game objects, occupancy, islands, building looks, placement, roads, logistics, island economy, population, walkers, building models, producer locations and trees, coastal placement, smoke, boats, production, carts, tile-aligned terrain, treasury, ships; `TestWorld` generates the real spawn island once for the tests that need voxels.
* `shaders/include/` shared GLSL (voxel lookup, scene uniforms, water, lighting, G-buffer); `shaders/render/` the three render passes and the float pass; `shaders/water/` ocean FFT and shore map passes; `shaders/grass/` grass animation; `shaders/people/` smoke puffs. Animation passes that write voxels on the GPU share `shaders/include/voxel_write.glsl`.

### Game Mechanics (Anno 1800 Style)
* **Population System:** Managed per-island via an `IslandEconomy` manager. Citizen tiers are processed sequentially (Farmers -> Workers -> Artisans -> Engineers -> Investors).
* **Deterministic Logic:** The production simulation runs on a decoupled `FixedUpdate(float tickRate)` loop (e.g., 10Hz). The render loop interpolates visual states.
* **Voxel Terrain:** Fixed-size chunks (32 × 32 × 32) storing 8-bit voxel IDs (`uint8_t`, up to 255 block types; buildings are `GameObject`s, not voxel IDs). Buildings, roads and ranges use a build grid of 12 x 12 voxel columns per tile (`TILE_SIZE` in `src/World/WorldConstants.h`); islands are made of whole tiles too, so coasts follow the grid. Building heights come from the type table. 

---

## 🎨 Code & Design Patterns

### 1. Voxel Grid & Custom Component Mapping
* **Data-Driven Separation:** The voxel grid only stores structural IDs. When an industrial building is placed, a `GameObject` is spawned with a `ProductionComponent` and a `VoxelAnchorComponent` linking it to its grid coordinates.
* **Pointer Safety:** Avoid storing raw pointers to components inside lists, as flat array reallocations will invalidate them. Use unique runtime IDs (`uint32_t`) or stable indices instead.

### 2. Memory & Performance Constraints
* **Eliminate MSVC Heap Fragmentation:** Strictly no `new`/`delete` or runtime `std::vector::push_back` operations within the main simulation tick or frame render loops. 
* **Pre-allocation:** Use `std::vector::reserve()` inside system setup routines or rely on static `std::array` sizes for fixed mechanics (like maximum factory input slots).

### 3. Voxel Rendering (GPU Ray Marching)
* There is no meshing: the world is ray-marched on the GPU by compute shaders (`default.comp`: trace, half-resolution shadow and shade passes) reading chunk voxel data from 3D textures through a page table.
* Keep chunk generation off the main thread (worker threads). When a building modifies voxels, update the CPU chunk and re-upload that chunk to its GPU slot; the 8³ brick occupancy mask is rebuilt on upload.

---

## 📝 Coding Standards & Style (MSVC Specific)

* **Compiler Pragmas:** Use `#pragma once` at the top of all headers. 
* **Warnings as Errors:** Code must compile cleanly with `/W4` and `/WX` enabled. Avoid unreferenced local variables and implicit downcasting warnings.
* **Naming Conventions:**
  * Classes/Structs: PascalCase (e.g., `ProductionComponent`)
  * Methods/Functions: PascalCase to align with common Windows/MSVC paradigms (e.g., `UpdateTick()`)
  * Member Variables: `m_` prefix (e.g., `m_InputStorage`)
* **Keywords:** Expressly mark class overrides with `override`. Use `constexpr` for production rate constants and tier requirements.

---

## 🚀 Common Workflows

### Adding a New Production Chain
1. Declare the new item identifier in `src/Simulation/ItemType.h`.
2. Configure its consumption ratios within `src/Economy/PopulationNeeds.cpp`.
3. Register the production chain layout (e.g., Iron Mine -> Charcoal Kiln -> Furnace -> Steelworks) in the game initialization tables.
4. Run the validation tests (`VoxelAnnoTests.exe`) to ensure the tick engine updates warehouse inventories correctly without crashing on thread synchronization points.
