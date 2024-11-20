#pragma once
#include <functional>
#include <string>
#include "imgui.h"

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

struct Style {
    ImVec4 green{0.11f, 0.63f, 0.38f, 1.f};
    ImVec4 yellow{0.83f, 0.58f, 0.09f, 1.f};
    ImVec4 red{0.75f, 0.25f, 0.36f, 1.f};
    ImVec4 blue{0.3019607961177826f, 0.4470588266849518f, 1.8901960849761963f, 1.f};
};

inline auto style() -> Style&
{
    static auto instance = Style{};
    return instance;
}

} // namespace ImGui::Notify
