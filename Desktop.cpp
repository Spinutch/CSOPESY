#include "Desktop.h"
#include "imgui/imgui.h"
#include <GLFW/glfw3.h>

void Desktop::DrawWallpaper(const ImVec2& min_p, const ImVec2& max_p) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // CHANGE TO THIS PART TO DIFF BACKGROUND (GRADIENT OR IMAGE)
    ImU32 col_top = IM_COL32(13, 17, 23, 255);       // Deep Charcoal Gray
    ImU32 col_bottom = IM_COL32(20, 30, 48, 255);    // Slate Blue
    
    draw_list->AddRectFilledMultiColor(min_p, max_p, col_top, col_top, col_bottom, col_bottom);
}

void Desktop::Render(GLFWwindow* window) {
    // 1. Fetch the main viewport geometry configurations
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // make the upcoming window to match the exact working size of the GLFW window
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    // 2. Setup flags to lock the window layer down and strip UI decorations
    ImGuiWindowFlags desktop_flags = 0;
    desktop_flags |= ImGuiWindowFlags_NoTitleBar;
    desktop_flags |= ImGuiWindowFlags_NoResize;
    desktop_flags |= ImGuiWindowFlags_NoMove;
    desktop_flags |= ImGuiWindowFlags_NoCollapse;
    desktop_flags |= ImGuiWindowFlags_NoScrollbar;
    desktop_flags |= ImGuiWindowFlags_NoSavedSettings;
    desktop_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus; // Keeps it anchored as a background
    desktop_flags |= ImGuiWindowFlags_NoBackground;        // Allows our DrawWallpaper gradient to show

    // Remove window padding to allow the background canvas drawing to sit flush with the screen edges
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DesktopCompositorShell", nullptr, desktop_flags);
    ImGui::PopStyleVar();

    // 3. Map out coordinate coordinates for relative sub-component placement
    ImVec2 min_p = viewport->WorkPos;
    ImVec2 max_p = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y);
    float layout_margin = 24.0f;

    // 4. Composition Execution order (Wallpaper -> Clock -> Power Button)
    DrawWallpaper(min_p, max_p);
    
    // Render child objects via composition
    m_Clock.Render(ImVec2(max_p.x, min_p.y), layout_margin);
    m_PowerButton.Render(window, min_p, max_p, layout_margin);

    ImGui::End();
}