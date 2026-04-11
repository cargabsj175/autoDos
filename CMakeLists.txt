cmake_minimum_required(VERSION 3.20)
project(AutoDOS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ── Output ────────────────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Sources ───────────────────────────────────────────────────────────────────
# miniz.h + miniz.c must be placed in src/ manually
# Download from: https://github.com/richgel999/miniz/releases

add_executable(AutoDOS WIN32
    src/main.cpp
    src/autodos.cpp
    src/miniz.c
)

target_include_directories(AutoDOS PRIVATE src)

target_link_libraries(AutoDOS PRIVATE
    nlohmann_json::nlohmann_json
    comctl32
    shell32
    comdlg32
    ole32
)

target_compile_definitions(AutoDOS PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0601
    UNICODE
    _UNICODE
)
