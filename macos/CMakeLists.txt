cmake_minimum_required(VERSION 3.20)
project(AutoDOS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Output ────────────────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── nlohmann/json (from Homebrew) ─────────────────────────────────────────────
find_package(nlohmann_json REQUIRED)

# ── SDL2 (from Homebrew) ──────────────────────────────────────────────────────
find_package(SDL2 REQUIRED)

# Find SDL2_ttf manually since Homebrew doesn't provide proper cmake config
find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h PATH_SUFFIXES SDL2)
find_library(SDL2_TTF_LIBRARY SDL2_ttf)

if(SDL2_TTF_INCLUDE_DIR AND SDL2_TTF_LIBRARY)
    set(SDL2_TTF_FOUND TRUE)
    set(SDL2_TTF_INCLUDE_DIRS ${SDL2_TTF_INCLUDE_DIR})
    set(SDL2_TTF_LIBRARIES ${SDL2_TTF_LIBRARY})
else()
    message(FATAL_ERROR "SDL2_ttf not found. Install with: brew install sdl2_ttf")
endif()

# ── Common sources ────────────────────────────────────────────────────────────
set(CORE_SOURCES
    autodos.cpp
    ../src/miniz.c
    shared/platform.cpp
)

set(CORE_INCLUDES
    .
    ../src
    shared
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

# ── CLI version ───────────────────────────────────────────────────────────────
add_executable(AutoDOS-cli
    cli/main.cpp
    ${CORE_SOURCES}
)

target_include_directories(AutoDOS-cli PRIVATE ${CORE_INCLUDES})

target_link_libraries(AutoDOS-cli PRIVATE
    nlohmann_json::nlohmann_json
)

# ── GUI version (SDL2 + ImGui) ────────────────────────────────────────────────
set(IMGUI_SOURCES
    gui/main.cpp
    gui/imgui.cpp
    gui/imgui_demo.cpp
    gui/imgui_draw.cpp
    gui/imgui_tables.cpp
    gui/imgui_widgets.cpp
    gui/imgui_impl_sdl2.cpp
    gui/imgui_impl_sdlrenderer2.cpp
    ${CORE_SOURCES}
)

set(IMGUI_INCLUDES
    ${CORE_INCLUDES}
    gui
)

add_executable(AutoDOS-gui ${IMGUI_SOURCES})

target_include_directories(AutoDOS-gui PRIVATE ${IMGUI_INCLUDES})

target_link_libraries(AutoDOS-gui PRIVATE
    nlohmann_json::nlohmann_json
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
)
