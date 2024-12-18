#pragma once
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include "imgui.h"

using namespace std::literals; // To write chrono values as 5s instead of std::chrono::seconds{5} // NOLINT(*global-names-in-headers)

namespace ImGuiNotify {

enum class Type {
    Success,
    Warning,
    Error,
    Info,
};

struct Notification {
    Type                                     type{Type::Info};
    std::string                              title{""};
    std::string                              content{""};
    std::function<void()>                    custom_imgui_content{}; /// ⚠ The lambda must capture everything by copy, it will be stored
    std::optional<std::chrono::milliseconds> duration{5s};           /// Set to std::nullopt to have an infinite duration. You then need to call ImGuiNotify::close(notification_id) manually.
    bool                                     is_closable{true};
    bool                                     hovering_keeps_notification_alive{true}; /// While this is true, if the user hovers the notification it will reset its lifetime
};

class NotificationId {
private:
    friend void render_windows();

    friend auto operator==(NotificationId const&, NotificationId const&) -> bool = default;

    inline static uint64_t _next_id{0};
    uint64_t               _id{_next_id++};
};

/// Returns a NotificationId that can be used to change() or close_after_small_delay() the notification (e.g. if it has an infinite duration)
/// This is thread-safe and can be called from any thread
auto send(Notification) -> NotificationId;

/// Changes the content of a notification that has already been sent
/// Does nothing if the notification has already been closed
/// This is thread-safe and can be called from any thread
void change(NotificationId, Notification);

/// Starts the closing animation after a given `delay`
/// Does nothing if the notification has already been closed
/// This is thread-safe and can be called from any thread
void close_after_small_delay(NotificationId, std::chrono::milliseconds delay = 1s);

/// Starts the closing animation immediately
/// Does nothing if the notification has already been closed
/// This is thread-safe and can be called from any thread
void close_immediately(NotificationId);

/// Must be called once per frame, during your normal imgui frame (before ImGui::Render())
void render_windows();

/// Must be called once when initializing imgui (if you use a custom font, call it just after adding that font)
/// If you don't use custom fonts, you must call ImGui::GetIO().Fonts->AddFontDefault() before calling ImGuiNotify::add_icons_to_current_font()
/// NB: you might have to tweak glyph_offset if the icons don't properly align with your custom font
void add_icons_to_current_font(float icon_size = 16.f, ImVec2 glyph_offset = {0.f, +4.f});

struct Style {
    ImVec4 color_success{0.11f, 0.63f, 0.38f, 1.f};
    ImVec4 color_warning{0.83f, 0.58f, 0.09f, 1.f};
    ImVec4 color_error{0.75f, 0.25f, 0.36f, 1.f};
    ImVec4 color_info{0.30f, 0.45f, 0.89f, 1.f};
    ImVec4 color_title_background{0.3f, 0.3f, 0.3f, 0.5f};

    float                     padding_x{20.f}; // Padding from the right of the window
    float                     padding_y{20.f}; // Padding from the bottom of the window
    float                     padding_between_notifications_y{10.f};
    float                     min_width{325.f};         // Forces notifications to have at least this width
    float                     border_width{5.f};        // Size of the border around the notifications
    std::chrono::milliseconds fade_in_duration{200ms};  // Duration of the transition when a notification appears
    std::chrono::milliseconds fade_out_duration{200ms}; // Duration of the transition when a notification disappears
    std::chrono::milliseconds change_duration{200ms};   // Duration of the transition when a notification changes (with ImGuiNotify::Change())
};

inline auto get_style() -> Style&
{
    static auto instance = Style{};
    return instance;
}

} // namespace ImGuiNotify
