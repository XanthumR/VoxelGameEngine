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
* **Run All Tests:** `.\build\bin\Debug\VoxelAnnoTests.exe`
* **Run Specific Simulation Test:** `.\build\bin\Debug\VoxelAnnoTests.exe --gtest_filter=SimulationTest.*`
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
* `src/World/` – CPU-side world: `WorldConstants`, `BlockTypes` (block IDs), `Chunk`, `VoxelWorld` (CPU chunk store), `TerrainGenerator` (islands, ocean, caves, trees, spawn search), `ChunkStreamer` (worker threads + streaming windows), `WorldEditor` (dig/place), `Raycast`, `VoxModel` (.vox loader), `GrassTufts`.
* `src/Rendering/` – GPU side: `GpuChunkCache` (pool textures, page table, brick masks), `VoxelRenderer` (trace/shadow/shade passes), `RenderTargets`, `RenderSettings`, `SkyLighting` (day-night), `OceanSimulation` (FFT waves), `ShoreMap` (coast distance field), `GrassAnimator`, `ShaderLoader` (supports `#include`).
* `src/Gameplay/` – `Camera` (`ICamera` interface), `StrategyCamera` (default Anno-style orbit camera), `FreeFlyCamera` (debug drone with collision, F1), `Picking` (cursor ray to hovered voxel), `DebugEditTool` (dig/place in free-fly).
* `src/UI/` – `DebugOverlay` (F3 window + minimap), `Hud` (crosshair + hotbar).
* `src/ThirdParty/` – `FastNoiseLite.h`, `glad.c`.
* `shaders/include/` shared GLSL (voxel lookup, scene uniforms, water, lighting, G-buffer); `shaders/render/` the three render passes; `shaders/water/` ocean FFT and shore map passes; `shaders/grass/` grass animation.

### Game Mechanics (Anno 1800 Style)
* **Population System:** Managed per-island via an `IslandEconomy` manager. Citizen tiers are processed sequentially (Farmers -> Workers -> Artisans -> Engineers -> Investors).
* **Deterministic Logic:** The production simulation runs on a decoupled `FixedUpdate(float tickRate)` loop (e.g., 10Hz). The render loop interpolates visual states.
* **Voxel Terrain:** Fixed-size chunks (32 × 32 × 32) storing 8-bit voxel IDs (`uint8_t`, up to 255 block types; buildings are `GameObject`s, not voxel IDs). 

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
