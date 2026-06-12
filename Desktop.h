#pragma once

struct GLFWwindow;
#include "Clock.h"
#include "PowerButton.h"

class Desktop {
private:
    Clock       m_Clock;
    PowerButton m_PowerButton;

    void DrawWallpaper(const ImVec2& min_p, const ImVec2& max_p);

public:
    Desktop() = default;
    ~Desktop() = default;

    // Orchestrates background execution layout configurations across sub-classes
    void Render(GLFWwindow* window);
};