#pragma once
#include <functional>
#include <string>

namespace ImGui::Notify {

enum class ToastType {
    None,
    Success,
    Warning,
    Error,
    Info,
};

struct Toast {
    ToastType   type{ToastType::None};
    std::string title{""};
    std::string content{""};

    int                   dismissTime{5000};
    std::function<void()> custom_imgui_content{}; // The lambda must capture everything by copy, it will be stored
};

void add(Toast);
/// Must be called once per frame, during your normal imgui frame (before ImGuiRenderblahblah)
void render();
/// Must be called once when initializing imgui,
void add_icons_to_current_font(float icons_size = 16.f);

} // namespace ImGui::Notify
