**HOW TO RUN THIS PROJECT**

**a. Install GLFW and Pkg-Config via Homebrew:**
     brew install glfw pkg-config
    **Verify Installation** 
    pkg-config --cflags --libs glfw3

**b. Ensure Project Structure** (click edit nalang to show actual format)

CSOPESY/
├── main.cpp            # Main initialization and engine loop
├── Desktop.h / .cpp    # OS Desktop Compositor layer (manages background & widgets)
├── Clock.h / .cpp      # Real-time System Clock widget
├── PowerButton.h/.cpp  # Shutdown button mechanism
└── imgui/              # Core Dear ImGui library folder
    ├── imgui.cpp
    ├── imgui.h
    └── backends/       # GLFW and OpenGL3 ImGui render backends
        ├── imgui_impl_glfw.cpp
        └── imgui_impl_opengl3.cpp
        
**c. Compile Command (Macbook)**
  1. g++ main.cpp Desktop.cpp Clock.cpp PowerButton.cpp imgui/imgui*.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp -I. -I./imgui -I./imgui/backends $(pkg-config --cflags --libs glfw3) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -std=c++17 -o emulator
  2. ./emulator
  3. A pop up window will appear showing the window
