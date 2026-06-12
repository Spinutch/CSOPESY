**HOW TO RUN EMULATOR**

**a. Installations**
1. brew install glfw pkg-config
2. brew install cmake
[Adjust installations according to your OS]

**b. Verify Installation** 
1. pkg-config --cflags --libs glfw3
2. cmake --version

**c. Ensure Project Structure**

CSOPESY/               <-- Project Root Folder
├── CMakeLists.txt     <-- Master Build Blueprint
├── main.cpp            # Engine Lifecycle Loop
├── Desktop.h / .cpp    # OS Desktop Compositor Layer (owns sub-components)
├── Clock.h / .cpp      # Encapsulated System Clock
├── PowerButton.h/.cpp  # Shutdown Event Intercept Button
├── imgui/              # Core Dear ImGui Library Files
│   ├── imgui.cpp
│   └── imgui.h
│   └── backends/       # Engine Render Pipelines
│       ├── imgui_impl_glfw.cpp
│       └── imgui_impl_opengl3.cpp
└── build/             # Dedicated Workspace for Output Binaries (Keep Empty initially)
        
**d. Compile Command**
1. cd downloads (or whereever CSOPESY project folder is in)
2. cd CSOPESY
3. cd build
4. cmake ..
5. cmake --build .
6. ./emulator

**e. resetting cache (optional)**
rm -rf *
cmake ..
cmake --build .
