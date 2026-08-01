cmake_minimum_required(VERSION 3.15)
project(CSOPESY_Emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)

# Automatically collect all local source files (.cpp) in the root folder,
# excluding old milestone entry points (main_1/2/3.cpp) and their scratch
# test file (test_memory.cpp) -- only main.cpp's int main() should be linked.
file(GLOB SOURCES "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*/(main_1|main_2|main_3|test_memory)\\.cpp$")

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