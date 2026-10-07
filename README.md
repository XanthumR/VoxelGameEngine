# Voxel Anno

A city-builder and trade game in the style of Anno 1800, built on a custom C++ voxel engine. Islands, buildings, people, carts and ships are all made of voxels and ray-marched on the GPU; the economy underneath runs on a fixed 10 Hz simulation.

## Features

- **Voxel renderer:** no meshing; compute shaders ray-march chunked voxel data with sun and moon shadows, a day-night cycle, an FFT ocean, animated grass and chimney smoke. Moving things (walkers, carts, boats, ships) are voxel objects with their own models that float on the waves.
- **Islands:** procedurally generated islands built from 12 x 12-voxel build tiles, with beaches, cliffs, caves and trees.
- **Building:** place, rotate, move (hold left click and drag) and demolish buildings on the tile grid. A green or red preview shows whether a spot is valid. Buildings are MagicaVoxel `.vox` models.
- **Roads and logistics:** drag L-shaped roads. Buildings need a road to a warehouse; producers' carts carry goods along the roads.
- **Population:** Farmers and Workers live in houses that grow, upgrade and shrink depending on how well their needs are met.
- **Production chains:** fishery, lumberjack and sawmill, sheep farm and framework knitter, pig farm and slaughterhouse. Workforce, location rules (coast, trees, pasture) and input buffers apply.
- **Economy:** coins from taxes minus building upkeep, build costs in coins and materials, game speed 0/1/2/4.
- **Ships and trade:** build ships at a harbor, sail them by right click, load and unload cargo, set up trade routes that ships sail on their own, and settle new islands.
- **Game UI:** Anno-style parchment panels made with RmlUi, in a Minecraft-like pixel font (Monocraft), with a picture of every building in the build menu.

## Controls

| Input | Action |
|---|---|
| WASD or arrow keys, screen edge | Pan the camera |
| Q / E | Rotate the camera |
| Mouse wheel | Zoom |
| Tab, 1-9 | Switch build menu tab, pick a building |
| R | Rotate the building to place |
| Left click | Place, select a building or ship |
| Hold left click on a building | Move it |
| Right click | Cancel; give a selected ship a move order |
| P, + / - | Pause, change game speed |
| Esc | Drop the build selection, or quit |
| F3 | Debug overlay and minimap |
| F1 | Free-fly debug camera (Tab frees the mouse) |

## Building

Requirements: Windows, Visual Studio 2022 (MSVC, C++20) and a GPU with OpenGL 4.4 and compute shaders.

Libraries that are not in the repository:

- **GLFW 3.4** (the prebuilt Windows release, with GLM in its `include` folder) at the `GlfwDir` property of `VoxelGameEngine.vcxproj`.
- **RmlUi 6.3** (the prebuilt Windows release) at the `RmlUiDir` property. Its DLLs are copied next to the executable after each build.
- **Dear ImGui** cloned into `imgui/` at the project root (used for the debug tools).

Override either path on the command line, for example `/p:GlfwDir=D:\libs\glfw /p:RmlUiDir=D:\libs\RmlUi`.

```powershell
msbuild VoxelGameEngine.sln -t:restore -p:RestorePackagesConfig=true   # Google Test, once
msbuild VoxelGameEngine.sln /p:Configuration=Release /p:Platform=x64 /m
.\x64\Release\VoxelGameEngine.exe   # run from the project root
.\x64\Release\VoxelAnnoTests.exe    # the simulation tests
```

Launch options: `--render-distance N`, `--render-scale F`, and for reproducible screenshots `--fixed-time T`, `--camera X Y Z YAW PITCH`, `--screenshot FILE SECONDS`.

## Project

- `CLAUDE.md` describes the architecture, the source layout and the coding rules.
- `docs/ROADMAP.md` holds the milestones. Milestones 1-7 are done; next are the higher population tiers (Artisans, Engineers, Investors) with NPC traders, then saving and loading.

## Credits

GLFW, GLM, glad, Dear ImGui, RmlUi, FastNoiseLite, Google Test, and the Monocraft font (SIL Open Font License).

## License

GNU General Public License v3.0, see `LICENSE.txt`.
