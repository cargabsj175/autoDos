cmake_minimum_required(VERSION 3.20)
project(AutoDOSLinux VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

find_package(nlohmann_json 3.11.3 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_executable(AutoDOS
    src/main_linux.cpp
    src/autodos.cpp
)

target_include_directories(AutoDOS PRIVATE src)
target_link_libraries(AutoDOS PRIVATE nlohmann_json::nlohmann_json)

add_custom_command(TARGET AutoDOS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/src/games.json
        $<TARGET_FILE_DIR:AutoDOS>/games.json
)

find_package(PkgConfig QUIET)
pkg_check_modules(GLFW3 QUIET glfw3)
find_package(OpenGL QUIET)

if(GLFW3_FOUND AND OpenGL_FOUND)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.90.9
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui_glfw_opengl STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(imgui_glfw_opengl PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${GLFW3_INCLUDE_DIRS}
    )

    target_link_libraries(imgui_glfw_opengl PUBLIC
        ${GLFW3_LIBRARIES}
        OpenGL::GL
    )

    add_executable(AutoDOS-GUI
        src/main_gui.cpp
        src/autodos.cpp
    )

    target_include_directories(AutoDOS-GUI PRIVATE
        src
        ${GLFW3_INCLUDE_DIRS}
    )

    target_link_libraries(AutoDOS-GUI PRIVATE
        nlohmann_json::nlohmann_json
        imgui_glfw_opengl
    )

    add_custom_command(TARGET AutoDOS-GUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/src/games.json
            $<TARGET_FILE_DIR:AutoDOS-GUI>/games.json
    )
else()
    message(WARNING "GLFW3/OpenGL not found; AutoDOS-GUI will not be built.")
endif()
