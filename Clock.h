#pragma once
#include "imgui/imgui.h"

class Clock {
public:
    Clock() = default;
    ~Clock() = default;

    // Evaluates time and draws the overlay at the top-right
    void Render(const ImVec2& screen_max_p, float margin = 20.0f);
};