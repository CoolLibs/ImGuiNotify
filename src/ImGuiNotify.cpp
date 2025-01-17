#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
//
#include <algorithm>
#include <mutex>
#include <optional>
#include <vector>
#include "IconsFontAwesome6.h"
#include "ImGuiNotify/ImGuiNotify.hpp"
#include "fa-solid-900.h"
#include "imgui_internal.h"

namespace ImGuiNotify {

class NotificationImpl {
public:
    explicit NotificationImpl(Notification notification)
        : _notification{std::move(notification)}
    {}

    auto color() const -> ImVec4
    {
        switch (_notification.type)
        {
        case Type::Success:
            return get_style().color_success;
        case Type::Warning:
            return get_style().color_warning;
        case Type::Error:
            return get_style().color_error;
        case Type::Info:
            return get_style().color_info;
        default:
            assert(false);
            return {1.f, 1.f, 1.f, 1.f};
        }
    }

    auto icon() const -> const char*
    {
        switch (_notification.type)
        {
        case Type::Success:
            return ICON_FA_CIRCLE_CHECK;
        case Type::Warning:
            return ICON_FA_TRIANGLE_EXCLAMATION;
        case Type::Error:
            return ICON_FA_CIRCLE_EXCLAMATION;
        case Type::Info:
            return ICON_FA_CIRCLE_INFO;
        default:
            assert(false);
            return "";
        }
    }

    auto has_content() const -> bool
    {
        return !_notification.content.empty() || _notification.custom_imgui_content;
    }
    auto content() const -> std::string const& { return _notification.content; }
    auto custom_imgui_content() const -> std::function<void()> const& { return _notification.custom_imgui_content; }
    auto title() const -> std::string const& { return _notification.title; }
    auto unique_id() const -> NotificationId const& { return _unique_id; }
    auto is_closable() const -> bool { return _notification.is_closable; }
    auto has_been_init() const -> bool { return _creation_time.has_value(); }

    auto elapsed_time() const
    {
        assert(has_been_init());
        return std::chrono::steady_clock::now() - *_creation_time;
    }

    auto duration_before_fade_out_starts() const
    {
        assert(has_been_init());
        assert(_notification.duration.has_value());
        return *_notification.duration + get_style().fade_in_duration - elapsed_time();
    }

    auto has_expired() const -> bool
    {
        return _remove_asap
               || (has_been_init()
                   && _notification.duration.has_value()
                   && elapsed_time() > *_notification.duration + get_style().fade_in_duration + get_style().fade_out_duration);
    }

    auto is_fading_out() const -> bool
    {
        return has_been_init()
               && _notification.duration.has_value()
               && elapsed_time() > *_notification.duration + get_style().fade_in_duration;
    }

    auto fade_percent() const -> float
    {
        if (!has_been_init())
            return 0.f;

        float const elapsed_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time()).count());
        float const fade_in_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(get_style().fade_in_duration).count());

        float percent = 1.f;

        if (elapsed_ms < fade_in_ms)
            percent = elapsed_ms / fade_in_ms;
        else if (_notification.duration.has_value())
        {
            float const duration_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(*_notification.duration).count());
            float const fade_out_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(get_style().fade_out_duration).count());
            if (elapsed_ms > duration_ms + fade_in_ms)
                percent = 1.f - (elapsed_ms - fade_in_ms - duration_ms) / fade_out_ms;
        }

        return std::clamp(percent, 0.f, 1.f);
    }

    void init_creation_time_ifn()
    {
        if (_creation_time.has_value())
            return;
        _creation_time = std::chrono::steady_clock::now();
    }

    void reset_creation_time()
    {
        if (!has_been_init())
            return;
        if (elapsed_time() > get_style().fade_in_duration)
            _creation_time = std::chrono::steady_clock::now() - get_style().fade_in_duration;
    }

    void set_hovered(bool is_hovered)
    {
        if (is_hovered && _notification.hovering_keeps_notification_alive)
            reset_creation_time();
    }

    void set_window_height(float height)
    {
        _window_height = height;
    }

    void close_after_at_most(std::chrono::milliseconds delay)
    {
        if (!has_been_init())
        {
            // Notification has not been shown on screen yet, so just change it's duration to make sure it is not bigger than `delay`
            if (_notification.duration.has_value())
                _notification.duration = std::min(*_notification.duration, delay);
            else
                _notification.duration = delay;
        }
        else if (!_notification.duration.has_value() || duration_before_fade_out_starts() > delay)
        {
            // Adapt the duration so that the fade out starts in exactly `delay`
            _notification.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                elapsed_time() - get_style().fade_in_duration + delay
            );
        }
    }

    void close_immediately()
    {
        _notification.hovering_keeps_notification_alive = false;
        if (!has_been_init())
            _remove_asap = true; // If we close immediately after sending, this prevents the notification from animating in, and then animating out immediately. This cancels all the animations.
        else
            close_after_at_most(0ms);
    }

    void change(Notification notification)
    {
        _notification = std::move(notification);
        reset_creation_time();
        if (_window_height.has_value())
        {
            _window_height_before_change = *_window_height;
            _time_of_change              = std::chrono::steady_clock::now();
        }
    }

    void apply_window_height_transition_ifn(float& window_height)
    {
        if (!_time_of_change.has_value())
            return;

        float const time_since_change_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - *_time_of_change).count());
        float const duration_ms          = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(get_style().change_duration).count());

        if (time_since_change_ms > duration_ms)
        {
            _time_of_change.reset();
            return;
        }

        float const t = time_since_change_ms / duration_ms;
        window_height = t * window_height + (1.f - t) * _window_height_before_change;
    }

private:
    Notification                                         _notification;
    std::optional<std::chrono::steady_clock::time_point> _creation_time{};
    bool                                                 _remove_asap{false};

    std::optional<float>                                 _window_height{};
    float                                                _window_height_before_change{};
    std::optional<std::chrono::steady_clock::time_point> _time_of_change{};

    NotificationId _unique_id{NotificationId::MakeValid{}};
};

static auto notifications() -> auto&
{
    static auto instance = std::vector<NotificationImpl>{};
    return instance;
}
// We don't want to lock while rendering the notifications in render_windows()
// because we want to allow the custom_imgui_content() of notifications to send / change / close notifications.
// So instead we delay all these actions so that they don't conflict while we are iterating on the list of notifications
static auto delayed_actions() -> auto&
{
    static auto instance = std::vector<std::function<void()>>{};
    return instance;
}
static auto delayed_actions_mutex() -> auto&
{
    static auto instance = std::mutex{};
    return instance;
}

auto send(Notification notification) -> NotificationId
{
    auto       notif_impl = NotificationImpl{std::move(notification)};
    auto const id         = notif_impl.unique_id();
    {
        auto lock = std::unique_lock{delayed_actions_mutex()};
        delayed_actions().emplace_back([notif_impl = std::move(notif_impl)]() {
            notifications().emplace_back(notif_impl);
        });
    }
    return id;
}

static void with_notification(NotificationId id, std::function<void(NotificationImpl&)> const& callback)
{
    auto const it = std::find_if(notifications().begin(), notifications().end(), [&](NotificationImpl const& notification) {
        return notification.unique_id() == id;
    });
    if (it == notifications().end())
        return;
    callback(*it);
}

void change(NotificationId id, Notification notification)
{
    auto lock = std::unique_lock{delayed_actions_mutex()};

    delayed_actions().emplace_back([id, notification = std::move(notification)]() mutable {
        with_notification(id, [&](NotificationImpl& notification_impl) {
            notification_impl.change(std::move(notification));
        });
    });
}

void close_after_small_delay(NotificationId id, std::chrono::milliseconds delay)
{
    auto lock = std::unique_lock{delayed_actions_mutex()};

    delayed_actions().emplace_back([id, delay]() {
        with_notification(id, [&](NotificationImpl& notification) {
            notification.close_after_at_most(delay);
        });
    });
}

void close_immediately(NotificationId id)
{
    auto lock = std::unique_lock{delayed_actions_mutex()};

    delayed_actions().emplace_back([id]() {
        with_notification(id, [&](NotificationImpl& notification) {
            notification.close_immediately();
        });
    });
}

static auto ImU32_from_ImVec4(ImVec4 color) -> ImU32
{
    return ImGui::GetColorU32(IM_COL32(
        color.x * 255.f,
        color.y * 255.f,
        color.z * 255.f,
        color.w * 255.f
    ));
}

static auto background(ImVec4 color, std::function<void()> const& widget) -> ImRect
{
    ImDrawList& draw_list = *ImGui::GetWindowDrawList();
    draw_list.ChannelsSplit(2);      // Allows us to draw the background rectangle behind the widget, even though the widget is drawn first.
    draw_list.ChannelsSetCurrent(1); // We must draw them in that order because we need to know the height of the widget before drawing the rectangle.

    ImVec2 const rectangle_start_pos = ImGui::GetCursorScreenPos() - ImGui::GetStyle().WindowPadding;

    widget();

    auto const rectangle_end_pos = ImVec2{
        rectangle_start_pos.x + ImGui::GetContentRegionAvail().x + 2.f * ImGui::GetStyle().WindowPadding.x,
        ImGui::GetCursorScreenPos().y
    };

    auto const rect = ImRect{rectangle_start_pos, rectangle_end_pos};

    draw_list.ChannelsSetCurrent(0);
    draw_list.AddRectFilled(
        rect.GetTL(),
        rect.GetBR(),
        ImU32_from_ImVec4(color)
    );
    draw_list.ChannelsMerge();

    return rect;
}

static auto close_button(ImRect const title_bar_rect) -> bool
{
    bool has_closed{false};

    ImGuiContext&      g{*GImGui};
    ImGuiWindow* const window{ImGui::GetCurrentWindow()};
    // Close button is on the Menu NavLayer and doesn't default focus (unless there's nothing else on that layer)
    // FIXME-NAV: Might want (or not?) to set the equivalent of ImGuiButtonFlags_NoNavFocus so that mouse clicks on standard title bar items don't necessarily set nav/keyboard ref?
    ImGuiItemFlags const item_flags_backup = g.CurrentItemFlags;
    g.CurrentItemFlags |= ImGuiItemFlags_NoNavDefaultFocus;
    window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;

    float const button_sz        = ImGui::GetFontSize();
    auto const  close_button_pos = ImVec2{
        title_bar_rect.Max.x - button_sz - ImGui::GetStyle().FramePadding.x,
        title_bar_rect.GetCenter().y - button_sz * 0.5f,
    };

    if (ImGui::CloseButton(window->GetID("#CLOSE"), close_button_pos))
        has_closed = true;

    window->DC.NavLayerCurrent = ImGuiNavLayer_Main;
    g.CurrentItemFlags         = item_flags_backup;

    return has_closed;
}

void render_windows()
{
    {
        auto lock = std::unique_lock{delayed_actions_mutex()};
        for (auto const& action : delayed_actions())
            action();
        delayed_actions().clear();
    }

    std::erase_if(notifications(), [](NotificationImpl const& notification) {
        return notification.has_expired();
    });

    float height = 0.f;
    for (size_t i = 0; i < notifications().size(); ++i)
    {
        ImVec2 const main_window_pos  = ImGui::GetMainViewport()->Pos;
        ImVec2 const main_window_size = ImGui::GetMainViewport()->Size;

        if (height > main_window_size.y - 100.f)
            break; // TODO(Notifications) Allow scrolling. eg switch to rendering just one window, with all notifications as child windows, and rely on imgui to do the scrollbar

        auto& notif = notifications()[i];
        notif.init_creation_time_ifn(); // Init creation time the first time a notification is shown, because if they are outside the window they might prevent it from showing for a while, and we don't want it to disappear immediately after appearing

        // Set window position and size
        ImGui::SetNextWindowPos(
            ImVec2{
                main_window_pos.x + main_window_size.x - get_style().padding_x,
                main_window_pos.y + main_window_size.y - get_style().padding_y - height
            },
            ImGuiCond_Always, ImVec2{1.f, 1.f}
        );
        ImGui::SetNextWindowSizeConstraints(
            ImVec2{get_style().min_width, 0.f}, // Min width
            ImVec2{FLT_MAX, FLT_MAX},
            [](ImGuiSizeCallbackData* data) {
                // in / out transition by cropping the window size
                NotificationImpl& notif = *reinterpret_cast<NotificationImpl*>(data->UserData); // NOLINT(*reinterpret-cast)
                data->DesiredSize.y *= notif.fade_percent();
                notif.apply_window_height_transition_ifn(data->DesiredSize.y);
            },
            (void*)&notif // NOLINT(*casting)
        );

        ImGui::PushStyleColor(ImGuiCol_Border, notif.color());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, get_style().border_width);
        ImGui::Begin(("##notification" + std::to_string(notif.unique_id()._id)).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);

        // Render over all other windows
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        // Keep alive if hovered
        notif.set_hovered(ImGui::IsWindowHovered());

        // Here we render the content
        {
            ImGui::PushTextWrapPos(ImGui::GetWindowWidth()); // Support multi-line text

            // Title bar
            auto const title_bar_rect = background(get_style().color_title_background, [&]() {
                ImGui::TextColored(notif.color(), "%s", notif.icon());
                ImGui::SameLine();
                ImGui::TextUnformatted(notif.title().c_str());
            });

            // Close button
            if (notif.is_closable())
            {
                if (close_button(title_bar_rect))
                    notif.close_immediately();
            }

            // Content
            if (notif.has_content())
            {
                // Add a small padding after the title
                ImGui::Dummy({0.f, 5.f});

                if (!notif.content().empty())
                    ImGui::TextUnformatted(notif.content().c_str());
                if (notif.custom_imgui_content())
                    notif.custom_imgui_content()();
            }

            ImGui::PopTextWrapPos();
        }

        // Update height for next notification
        float const window_height = ImGui::GetWindowHeight();
        notif.set_window_height(window_height);
        height += window_height + get_style().padding_between_notifications_y * notif.fade_percent();

        // End
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

void add_icons_to_current_font(float icon_size, ImVec2 glyph_offset)
{
    static constexpr ImWchar iconsRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0}; // NOLINT(*avoid-c-arrays)
    ImFontConfig             iconsConfig{};
    iconsConfig.MergeMode   = true;
    iconsConfig.PixelSnapH  = true;
    iconsConfig.GlyphOffset = glyph_offset;
    // iconsConfig.GlyphMinAdvanceX = icon_size; // Use if you want to make the icons monospaced
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size, icon_size, &iconsConfig, iconsRanges);
}

} // namespace ImGuiNotify
