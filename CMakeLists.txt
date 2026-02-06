cmake_minimum_required(VERSION 3.15)
project(FFTWater LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)
include_directories(external/glad/include)
include_directories(external/glfw-3.4/include)
include_directories(external/glm)

include_directories(external/imgui)
include_directories(external/imgui/backends)

include_directories(external/cgltf)

include_directories(external/sgp4/libsgp4)


include_directories(external/stb)

file(GLOB SOURCES "src/*.cpp" "src/*.h")

set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp

    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(FFTWater ${SOURCES} ${IMGUI_SOURCES})

add_library(glad STATIC external/glad/src/glad.c)
target_link_libraries(FFTWater glad)

add_subdirectory(external/glfw-3.4)
target_link_libraries(FFTWater glfw)

add_subdirectory(external/sgp4/libsgp4)
target_link_libraries(FFTWater sgp4)

if (WIN32)
    target_link_libraries(FFTWater opengl32)
elseif (APPLE)
    find_library(OpenGL_LIBRARY OpenGL)
    target_link_libraries(FFTWater ${OpenGL_LIBRARY})
else()
    target_link_libraries(FFTWater GL dl pthread X11 Xrandr Xi)
endif()
