**HOW TO RUN THIS PROJECT**

**a. Installations:**
     brew install glfw pkg-config
     brew install cmake
    **Verify Installation** 
    pkg-config --cflags --libs glfw3
    cmake --version

**b. Ensure Project Structure**

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
        
**c. Compile Command**
    cd downloads (or wherever CSOPESY project is in)
    cd CSOPESY
    cd build
    cmake ..
    cmake --build .
    ./emulator

resetting cache:
rm -rf *
cmake ..
cmake --build .