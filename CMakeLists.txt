cmake_minimum_required(VERSION 3.24)
project(VoxelAnno LANGUAGES C CXX)

# Voxel Anno: the game (VoxelGameEngine) and its simulation tests (VoxelAnnoTests).
# Every library is downloaded and built here (FetchContent); on Linux FreeType comes from the
# system when it is installed. Build from the project root:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release --parallel
# and run from the project root, so shaders/ and assets/ are found.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Everything static: no DLLs to copy

option(VOXEL_ANNO_TESTS "Build the simulation tests" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# --- GLFW (window, input, OpenGL context) ---
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# X11 only (Wayland desktops run it through XWayland); turn on for native Wayland
option(GLFW_BUILD_WAYLAND "Build GLFW's Wayland support" OFF)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)

# --- GLM (header-only math) ---
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 0.9.9.8 GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-build) # Headers only: its own CMake project is not needed

# --- Dear ImGui (debug tools; no CMake project of its own) ---
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.90 GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-build)

# --- RmlUi (the game UI) and FreeType for its fonts ---
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(rmlui GIT_REPOSITORY https://github.com/mikke89/RmlUi.git GIT_TAG 6.3 GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw glm imgui)

find_package(Freetype QUIET)
if(NOT TARGET Freetype::Freetype)
    # Not installed (Windows, or Linux without libfreetype-dev): build it, without its optional codecs
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    FetchContent_Declare(freetype GIT_REPOSITORY https://github.com/freetype/freetype.git GIT_TAG VER-2-13-3 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
FetchContent_MakeAvailable(rmlui)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

# --- Sources ---
# The deterministic game logic, shared by the game and the tests (no OpenGL, no window)
set(LOGIC_SOURCES
    src/Economy/IslandEconomy.cpp
    src/Economy/PopulationNeeds.cpp
    src/Economy/PopulationSystem.cpp
    src/Economy/Production.cpp
    src/Economy/Treasury.cpp
    src/Gameplay/Carts.cpp
    src/Gameplay/FigureModels.cpp
    src/Gameplay/FishingBoats.cpp
    src/Gameplay/Smoke.cpp
    src/Gameplay/Walkers.cpp
    src/Simulation/BuildingLook.cpp
    src/Simulation/BuildingModels.cpp
    src/Simulation/GameClock.cpp
    src/Simulation/GameObjects.cpp
    src/Simulation/IslandRegistry.cpp
    src/Simulation/Logistics.cpp
    src/Simulation/OccupancyGrid.cpp
    src/Simulation/Placement.cpp
    src/Simulation/ProducerLocation.cpp
    src/Simulation/RoadNetwork.cpp
    src/Simulation/Ships.cpp
    src/Simulation/Simulation.cpp
    src/Simulation/TreeRegistry.cpp
    src/World/BuildingModel.cpp
    src/World/TerrainGenerator.cpp
    src/World/VoxModel.cpp
    src/World/VoxelWorld.cpp)

set(GAME_SOURCES
    src/main.cpp
    src/Core/Application.cpp
    src/Core/LaunchOptions.cpp
    src/Core/Screenshot.cpp
    src/Gameplay/BuildTool.cpp
    src/Gameplay/DebugEditTool.cpp
    src/Gameplay/FreeFlyCamera.cpp
    src/Gameplay/Picking.cpp
    src/Gameplay/RoadTool.cpp
    src/Gameplay/ShipControl.cpp
    src/Gameplay/StrategyCamera.cpp
    src/Rendering/FigureRenderer.cpp
    src/Rendering/GpuChunkCache.cpp
    src/Rendering/GpuTimers.cpp
    src/Rendering/GrassAnimator.cpp
    src/Rendering/OceanSimulation.cpp
    src/Rendering/RenderTargets.cpp
    src/Rendering/ShaderLoader.cpp
    src/Rendering/ShoreMap.cpp
    src/Rendering/SkyLighting.cpp
    src/Rendering/TileOverlay.cpp
    src/Rendering/VoxelRenderer.cpp
    src/UI/BuildMenu.cpp
    src/UI/BuildingInfo.cpp
    src/UI/DebugOverlay.cpp
    src/UI/GameUi.cpp
    src/UI/Hud.cpp
    src/UI/InfoLines.cpp
    src/UI/IslandPanel.cpp
    src/UI/RmlBackend.cpp
    src/UI/ShipPanel.cpp
    src/UI/TopBar.cpp
    src/UI/TradeRoutes.cpp
    src/UI/WorldMarkers.cpp
    src/World/ChunkStreamer.cpp
    src/World/Raycast.cpp
    src/World/WorldEditor.cpp
    src/ThirdParty/glad.c)

# --- The game ---
add_executable(VoxelGameEngine ${GAME_SOURCES} ${LOGIC_SOURCES})
target_include_directories(VoxelGameEngine PRIVATE src src/ThirdParty ${glm_SOURCE_DIR} ${rmlui_SOURCE_DIR}/Backends)
target_link_libraries(VoxelGameEngine PRIVATE glfw imgui RmlUi::RmlUi OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS})
if(MSVC)
    target_compile_options(VoxelGameEngine PRIVATE /W3 /MP)
    target_compile_definitions(VoxelGameEngine PRIVATE _CRT_SECURE_NO_WARNINGS) # fopen and friends
else()
    target_compile_options(VoxelGameEngine PRIVATE -Wall)
endif()
# Visual Studio starts it in the project root, where shaders/ and assets/ are
set_target_properties(VoxelGameEngine PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VoxelGameEngine)

# --- The tests ---
if(VOXEL_ANNO_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.2 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest)

    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(VoxelAnnoTests ${TEST_SOURCES} ${LOGIC_SOURCES})
    target_include_directories(VoxelAnnoTests PRIVATE src tests ${glm_SOURCE_DIR})
    target_compile_definitions(VoxelAnnoTests PRIVATE ASSET_DIRECTORY="${CMAKE_SOURCE_DIR}/assets")
    target_link_libraries(VoxelAnnoTests PRIVATE GTest::gtest Threads::Threads)
    if(MSVC)
        target_compile_options(VoxelAnnoTests PRIVATE /W4 /WX /external:anglebrackets /external:W0 /MP)
    else()
        target_compile_options(VoxelAnnoTests PRIVATE -Wall -Wextra)
    endif()
    set_target_properties(VoxelAnnoTests PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    enable_testing()
    add_test(NAME VoxelAnnoTests COMMAND VoxelAnnoTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
