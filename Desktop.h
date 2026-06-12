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

    void DrawWallpaper(const ImVec2 &min_p, const ImVec2 &max_p);
    bool LoadTextureFromFile(const char *filename, unsigned int *out_texture, int *out_width, int *out_height);

public:
    Desktop();
    ~Desktop();

    void Render(GLFWwindow *window);
};