#pragma once
#include <functional>
#include <string>

namespace ImGui::Notify {

static constexpr float NOTIFY_PADDING_X         = 20.f;       // Bottom-left X padding
static constexpr float NOTIFY_PADDING_Y         = 1.f * 20.f; // Bottom-left Y padding
static constexpr float NOTIFY_PADDING_MESSAGE_Y = 1.f * 10.f; // Padding Y between each message
static constexpr float NOTIFY_MIN_WIDTH         = 200.f;
#define NOTIFY_FADE_IN_OUT_TIME 200             // Fade in and out duration
#define NOTIFY_DEFAULT_DISMISS  5000            // Auto dismiss after X ms (default, applied only of no data provided in constructors)
#define NOTIFY_USE_SEPARATOR    false           // If true, a separator will be rendered between the title and the content
static constexpr size_t NOTIFY_RENDER_LIMIT{5}; // Max number of toasts rendered at the same time. Set to 0 for unlimited

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

    int                   dismissTime{NOTIFY_DEFAULT_DISMISS};
    std::function<void()> custom_imgui_content{}; // The lambda must capture everything by copy, it will be stored
};

void add(Toast);
/// Must be called once per frame, during your normal imgui frame (before ImGuiRenderblahblah)
void render();
/// Must be called once when initializing imgui,
void add_icons_to_current_font(float icons_size = 16.f);

} // namespace ImGui::Notify
