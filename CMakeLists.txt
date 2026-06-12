cmake_minimum_required(VERSION 3.15)
project(CSOPESY_Emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)

# Automatically collect all local source files (.cpp) in the root folder
file(GLOB SOURCES "*.cpp")

# include ImGui source files
file(GLOB IMGUI_SOURCES 
    "imgui/imgui*.cpp"
    "imgui/backends/imgui_impl_glfw.cpp"
    "imgui/backends/imgui_impl_opengl3.cpp"
)

set(ALL_SOURCES ${SOURCES} ${IMGUI_SOURCES})
add_executable(emulator ${ALL_SOURCES})

target_include_directories(emulator PRIVATE
    .
    ./imgui
    ./imgui/backends
    ${GLFW_INCLUDE_DIRS}
)

target_link_libraries(emulator PRIVATE
    PkgConfig::GLFW
    "-framework OpenGL"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
)