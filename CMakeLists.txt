cmake_minimum_required(VERSION 3.30)

project(pg2 VERSION 2025.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)

if(WIN32)
    # Na Windows zkusíme najít OpenCV standardně, jinak použijeme pevnou cestu
    find_package(OpenCV CONFIG QUIET)
    if(NOT OpenCV_FOUND)
        find_package(OpenCV CONFIG REQUIRED PATHS "C:/opencv/opencv/build")
    endif()
elseif(UNIX)
    # Na Linuxu použijeme systémové OpenCV
    find_package(OpenCV REQUIRED)
endif()

set(PROJECT_SOURCES
    # helloworld.cpp
    main.cpp
    assets.hpp
    app.cpp
    app.hpp
    ShaderProgram.cpp
    ShaderProgram.hpp
    OBJloader.cpp
    OBJloader.hpp
    Model.hpp
    Mesh.hpp
    Texture.cpp
    Texture.hpp
    camera.hpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    fmt::fmt
    GLEW::GLEW
    glfw
    imgui::imgui
    OpenGL::GL
    glm::glm
    nlohmann_json::nlohmann_json
    ${OpenCV_LIBS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

