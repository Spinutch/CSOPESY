#include "Clock.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

void Clock::Render(const ImVec2& screen_max_p, float margin) {
    // Fetch system precision time
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    std::tm* local_time = std::localtime(&time_now);
    
    // Format to display standard string
    std::stringstream ss;
    ss << std::put_time(local_time, "%Y-%m-%d  %H:%M:%S");
    std::string clock_text = ss.str();

    // Dynamically calculate string render width to offset right alignment boundaries
    float text_width = ImGui::CalcTextSize(clock_text.c_str()).x;
    ImVec2 text_pos = ImVec2(screen_max_p.x - text_width - margin, screen_max_p.y + margin);
    
    ImGui::SetCursorScreenPos(text_pos);
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", clock_text.c_str());
}