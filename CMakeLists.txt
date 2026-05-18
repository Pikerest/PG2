cmake_minimum_required(VERSION 3.30)

# Main build file for the PG2 OpenGL game project.
# The game code is intentionally split by responsibility:
# app.cpp = lifecycle/frame loop, app_assets.cpp = scene creation,
# app_gameplay.cpp = gameplay state, app_render.cpp = UI/special rendering,
# app_input.cpp = callbacks, app_collision.cpp = movement collision.
project(pg2 VERSION 2025.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release when the generator does not provide multi-config builds
# such as Visual Studio's Debug/Release selector.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Third-party dependencies are resolved through vcpkg/OpenCV installation.
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)

if(WIN32)
    find_package(OpenCV CONFIG QUIET)
    if(NOT OpenCV_FOUND)
        find_package(OpenCV CONFIG REQUIRED PATHS "C:/opencv/opencv/build")
    endif()
elseif(UNIX)
    find_package(OpenCV REQUIRED)
endif()

# Keep every project-owned source/header visible in IDE generators.
# miniaudio.h is header-only but listed here so it appears in the project tree.
set(PROJECT_SOURCES
    main.cpp
    assets.hpp
    app.cpp
    app.hpp
    app_input.cpp
    app_collision.cpp
    app_gameplay.cpp
    app_assets.cpp
    app_render.cpp
    ShaderProgram.cpp
    ShaderProgram.hpp
    OBJloader.cpp
    OBJloader.hpp
    Model.hpp
    Mesh.hpp
    Texture.cpp
    Texture.hpp
    camera.hpp
    audio.cpp
    audio.hpp
    miniaudio.h
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# Expose the source directory to C++ so the app can find assets even when
# launched from a build folder instead of the repository root.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PG2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC)
    # Larger template/debug information memory budget for Visual Studio builds.
    target_compile_options(${PROJECT_NAME} PRIVATE /Zm300)
endif()

# Link rendering, math, UI, JSON, and image-loading libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE
    GLEW::GLEW
    glfw
    imgui::imgui
    OpenGL::GL
    glm::glm
    nlohmann_json::nlohmann_json
    ${OpenCV_LIBS}
)

# Local includes make project headers available without long relative paths.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

