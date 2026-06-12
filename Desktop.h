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
    
    void Render(GLFWwindow* window);
};