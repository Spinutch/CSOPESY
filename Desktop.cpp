#include "Desktop.h"
#include "imgui/imgui.h"
#include <GLFW/glfw3.h>

// Define stb_image implementation exactly once
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Desktop::Desktop()
{
    LoadTextureFromFile("../mac_wallpaper2.png", &m_WallpaperTexture, &m_WallpaperWidth, &m_WallpaperHeight);
}

Desktop::~Desktop()
{
    // Free the GPU memory when the OS shuts down
    if (m_WallpaperTexture)
    {
        glDeleteTextures(1, &m_WallpaperTexture);
    }
}

bool Desktop::LoadTextureFromFile(const char *filename, unsigned int *out_texture, int *out_width, int *out_height)
{
    int image_width = 0;
    int image_height = 0;
    int image_channels = 0;

    // Load pixel data from file
    unsigned char *image_data = stbi_load(filename, &image_width, &image_height, &image_channels, 4);
    if (image_data == nullptr)
        return false;

    // Create an OpenGL texture
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into the GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    // Free the raw CPU-side pixel data
    stbi_image_free(image_data);

    *out_texture = image_texture;
    *out_width = image_width;
    *out_height = image_height;

    return true;
}

void Desktop::DrawWallpaper(const ImVec2 &min_p, const ImVec2 &max_p)
{
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    if (m_WallpaperTexture)
    {
        // Draw the image if loaded successfully
        draw_list->AddImage((ImTextureID)(intptr_t)m_WallpaperTexture, min_p, max_p);
    }
    else
    {
        // Fallback to the Windows/Mac gradient if the image is missing
        ImU32 col_top = IM_COL32(0, 120, 215, 255);
        ImU32 col_bottom = IM_COL32(94, 92, 230, 255);
        draw_list->AddRectFilledMultiColor(min_p, max_p, col_top, col_top, col_bottom, col_bottom);
    }
}

void Desktop::Render(GLFWwindow *window)
{
    // Fetch the main viewport geometry configurations
    ImGuiViewport *viewport = ImGui::GetMainViewport();

    // make the upcoming window to match the exact working size of the GLFW window
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    // Setup flags to lock the window layer down and strip UI decorations
    ImGuiWindowFlags desktop_flags = 0;
    desktop_flags |= ImGuiWindowFlags_NoTitleBar;
    desktop_flags |= ImGuiWindowFlags_NoResize;
    desktop_flags |= ImGuiWindowFlags_NoMove;
    desktop_flags |= ImGuiWindowFlags_NoCollapse;
    desktop_flags |= ImGuiWindowFlags_NoScrollbar;
    desktop_flags |= ImGuiWindowFlags_NoSavedSettings;
    desktop_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    desktop_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DesktopCompositorShell", nullptr, desktop_flags);
    ImGui::PopStyleVar();

    ImVec2 min_p = viewport->WorkPos;
    ImVec2 max_p = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y);
    float layout_margin = 24.0f;

    // Composition Execution order (Wallpaper -> Clock -> Power Button)
    DrawWallpaper(min_p, max_p);
    m_Clock.Render(ImVec2(max_p.x, min_p.y), layout_margin);

    RenderTaskbar(window, min_p, max_p);

    ImGui::End();

    // so they can be dragged around and focused freely
    if (m_ShowPlaceholder1) RenderPlaceholder1();
    if (m_ShowPlaceholder2) RenderPlaceholder2();
    if (m_ShowTaskManager)  RenderTaskManager();
}

void Desktop::RenderTaskbar(GLFWwindow *window, const ImVec2 &min_p, const ImVec2 &max_p)
{
    float taskbarHeight = 48.0f;

    ImGui::SetNextWindowPos(ImVec2(min_p.x, max_p.y - taskbarHeight));
    ImGui::SetNextWindowSize(ImVec2(max_p.x - min_p.x, taskbarHeight));

    ImGuiWindowFlags taskbar_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

    ImGui::Begin("TaskbarPanel", nullptr, taskbar_flags);

    // --- Left-Aligned Taskbar Actions ---
    if (ImGui::Button("INIT", ImVec2(60, 28))) { m_ShowPlaceholder1 = !m_ShowPlaceholder1; }
    ImGui::SameLine();

    if (ImGui::Button("SRCH", ImVec2(60, 28))) { m_ShowPlaceholder2 = !m_ShowPlaceholder2; }
    ImGui::SameLine();

    if (ImGui::Button("TaskMgr", ImVec2(100, 28))) { m_ShowTaskManager = !m_ShowTaskManager; }

    // --- Right-Aligned Power Button ---
    // We pass the taskbar coordinates, and our updated PowerButton handles the rest!
    ImVec2 taskbarMin = ImVec2(min_p.x, max_p.y - taskbarHeight);
    ImVec2 taskbarMax = max_p;

    // 6.0f margin keeps it looking perfectly centered vertically within the 48px bar
    m_PowerButton.Render(window, taskbarMin, taskbarMax, 6.0f);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void Desktop::RenderPlaceholder1()
{
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("System Initialization Settings", &m_ShowPlaceholder1))
    {
        ImGui::Text("Welcome to OperatingOS v1.0 Initialization");
        ImGui::Separator();
        ImGui::Text("Kernel State: Online");
        ImGui::Text("Subsystems loaded successfully.");
    }
    ImGui::End();
}

void Desktop::RenderPlaceholder2()
{
    ImGui::SetNextWindowSize(ImVec2(350, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("File Search Indexer", &m_ShowPlaceholder2))
    {
        ImGui::Text("Search Directory: /root/system32/");
        static char searchBuffer[128] = "";
        ImGui::InputText("Query", searchBuffer, IM_ARRAYSIZE(searchBuffer));
        if (ImGui::Button("Index Assets")) {
            // Placeholder trigger action
        }
    }
    ImGui::End();
}

// --- Component 3: The Task Manager ---
void Desktop::RenderTaskManager()
{
    ImGui::SetNextWindowSize(ImVec2(450, 300), ImGuiCond_FirstUseEver);

    // Passing &m_ShowTaskManager allows the standard window 'X' close button to work natively
    if (ImGui::Begin("Task Manager", &m_ShowTaskManager))
    {
        ImGui::Text("Performance Metrics System Process List:");
        ImGui::Separator();

        // Construct table with 3 distinct columns
        if (ImGui::BeginTable("ProcessTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Process Name");
            ImGui::TableSetupColumn("CPU Usage");
            ImGui::TableSetupColumn("Memory Allocation");
            ImGui::TableHeadersRow();

            // Row 1: System Compositor Shell
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("DesktopCompositorShell");
            ImGui::TableSetColumnIndex(1); ImGui::Text("1.2 %%");
            ImGui::TableSetColumnIndex(2); ImGui::Text("14,240 KB");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("TomodachiLife.exe");
            ImGui::TableSetColumnIndex(1); ImGui::Text("42.7 %%");
            ImGui::TableSetColumnIndex(2); ImGui::Text("245,680 KB");

            // Row 3: OS Idle Runtime
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("System Idle Process");
            ImGui::TableSetColumnIndex(1); ImGui::Text("56.1 %%");
            ImGui::TableSetColumnIndex(2); ImGui::Text("16 KB");

            ImGui::EndTable();
        }
    }
    ImGui::End();
}