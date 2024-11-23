#pragma once
#include <chrono>
#include <functional>
#include <string>
#include "imgui.h"

namespace ImGuiNotify {

using namespace std::literals; // To write chrono values as 5s instead of std::chrono::seconds{5}

enum class Type {
    Success,
    Warning,
    Error,
    Info,
};

struct Notification {
    Type                      type{Type::Info};
    std::string               title{""};
    std::string               content{""};
    std::function<void()>     custom_imgui_content{}; // ⚠ The lambda must capture everything by copy, it will be stored
    std::chrono::milliseconds duration{5s};
};

void send(Notification);
/// Must be called once per frame, during your normal imgui frame (before ImGui::Render())
void render_windows();
/// Must be called once when initializing imgui (if you use a custom font, call it just after adding that font)
void add_icons_to_current_font(float icons_size = 16.f);

struct Style {
    ImVec4 success{0.11f, 0.63f, 0.38f, 1.f};
    ImVec4 warning{0.83f, 0.58f, 0.09f, 1.f};
    ImVec4 error{0.75f, 0.25f, 0.36f, 1.f};
    ImVec4 info{0.30f, 0.45f, 0.89f, 1.f};
    ImVec4 title_background{0.3f, 0.3f, 0.3f, 0.5f};

    float                     padding_x{20.f}; // Padding from the right of the window
    float                     padding_y{20.f}; // Padding from the bottom of the window
    float                     padding_between_notifications_y{10.f};
    float                     min_width{275.f};         // Forces notifications to have at least this width
    float                     border_width{5.f};        // Size of the border around the notifications
    std::chrono::milliseconds fade_in_duration{200ms};  // Duration of the transition when a notification appears
    std::chrono::milliseconds fade_out_duration{200ms}; // Duration of the transition when a notification disappears
    size_t                    render_limit{5};          // Max number of notifications rendered at the same time
};

inline auto get_style() -> Style&
{
    static auto instance = Style{};
    return instance;
}

} // namespace ImGuiNotify
