#pragma once

struct GLFWwindow;
#include "imgui/imgui.h"

class PowerButton {
public:
    PowerButton() = default;
    ~PowerButton() = default;

    // Renders the button and binds callback mechanisms to close the GLFW runtime window safely
    void Render(GLFWwindow* window, const ImVec2& screen_min_p, const ImVec2& screen_max_p, float margin = 20.0f);
};