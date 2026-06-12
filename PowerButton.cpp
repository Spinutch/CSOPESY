#include "PowerButton.h"
#include <GLFW/glfw3.h>

void PowerButton::Render(GLFWwindow* window, const ImVec2& screen_min_p, const ImVec2& screen_max_p, float margin) {
    float btn_width = 80.0f;
    float btn_height = 35.0f;
    
    // Position cursor at bottom-left corner offset by margins
    ImGui::SetCursorScreenPos(ImVec2(screen_min_p.x + margin, screen_max_p.y - btn_height - margin));

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.05f, 0.05f, 1.0f));

    if (ImGui::Button("PWR", ImVec2(btn_width, btn_height))) {
        glfwSetWindowShouldClose(window, GLFW_TRUE); // Tells main rendering cycle loop to collapse gracefully
    }
    
    ImGui::PopStyleColor(3);
}