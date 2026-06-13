#pragma once

struct GLFWwindow;
#include "Clock.h"
#include "PowerButton.h"

class Desktop
{
private:
    Clock m_Clock;
    PowerButton m_PowerButton;

    unsigned int m_WallpaperTexture = 0;
    int m_WallpaperWidth = 0;
    int m_WallpaperHeight = 0;

    bool m_ShowPlaceholder1 = false;
    bool m_ShowPlaceholder2 = false;
    bool m_ShowTaskManager = false;

    void DrawWallpaper(const ImVec2 &min_p, const ImVec2 &max_p);
    bool LoadTextureFromFile(const char *filename, unsigned int *out_texture, int *out_width, int *out_height);

    void RenderTaskbar(GLFWwindow *window, const ImVec2 &min_p, const ImVec2 &max_p);
    void RenderPlaceholder1();
    void RenderPlaceholder2();
    void RenderTaskManager();

public:
    Desktop();
    ~Desktop();

    void Render(GLFWwindow *window);
};