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

namespace {
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
    auto has_been_init() const -> bool { return _creation_time.has_value(); }

    auto elapsed_time() const
    {
        assert(has_been_init());
        return std::chrono::steady_clock::now() - *_creation_time;
    }

    auto has_expired() const -> bool
    {
        return has_been_init()
               && _notification.duration.has_value()
               && elapsed_time() > *_notification.duration + get_style().fade_in_duration + get_style().fade_out_duration;
    }

    auto is_fading_out() const -> bool
    {
        return has_been_init()
               && _notification.duration.has_value()
               && elapsed_time() > *_notification.duration + get_style().fade_in_duration;
    }

    auto fade_percent() const -> float
    {
        float const elapsed_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time()).count());
        float const fade_in_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(get_style().fade_in_duration).count());

        float percent = 1.f;

        if (elapsed_ms < fade_in_ms)
            percent = elapsed_ms / fade_in_ms;
        else if (_notification.duration.has_value())
        {
            if (_is_hovered)
                return 1.f; // Prevent jitter when a notification is hovered and we call close() on this notification

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
        if (elapsed_time() > get_style().fade_in_duration)
            _creation_time = std::chrono::steady_clock::now() - get_style().fade_in_duration;
    }

    void set_hovered(bool is_hovered)
    {
        _is_hovered = is_hovered;
        if (is_hovered)
            reset_creation_time();
    }

    void close(std::chrono::milliseconds delay)
    {
        _notification.duration = delay;
        reset_creation_time();
    }

private:
    Notification                                         _notification;
    std::optional<std::chrono::steady_clock::time_point> _creation_time{};
    bool                                                 _is_hovered{false};

    NotificationId _unique_id{};
};
} // namespace

static auto notifications() -> std::vector<NotificationImpl>&
{
    static auto instance = std::vector<NotificationImpl>{};
    return instance;
}
static auto notifications_mutex() -> std::mutex&
{
    static auto instance = std::mutex{};
    return instance;
}

auto send(Notification notification) -> NotificationId
{
    auto lock = std::unique_lock{notifications_mutex()};
    notifications().emplace_back(std::move(notification));
    return notifications().back().unique_id();
}

void close(NotificationId id, std::chrono::milliseconds delay)
{
    auto       lock = std::unique_lock{notifications_mutex()};
    auto const it   = std::find_if(notifications().begin(), notifications().end(), [&](NotificationImpl const& notification) {
        return notification.unique_id() == id;
    });
    if (it == notifications().end())
        return;
    it->close(delay);
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

static void background(ImVec4 color, std::function<void()> const& widget)
{
    ImDrawList& draw_list = *ImGui::GetWindowDrawList();
    draw_list.ChannelsSplit(2);                                     // Allows us to draw the background rectangle behind the widget,
    draw_list.ChannelsSetCurrent(1);                                // even though the widget is drawn first.
    ImVec2 const rectangle_start_pos = ImGui::GetCursorScreenPos(); // We must draw them in that order because we need to know the height of the widget before drawing the rectangle.

    widget();

    auto const rectangle_end_pos = ImVec2{
        rectangle_start_pos.x + ImGui::GetContentRegionAvail().x,
        ImGui::GetCursorScreenPos().y
    };
    draw_list.ChannelsSetCurrent(0);
    draw_list.AddRectFilled(
        rectangle_start_pos - ImGui::GetStyle().WindowPadding,
        rectangle_end_pos + ImVec2{ImGui::GetStyle().WindowPadding.x, 0.f},
        ImU32_from_ImVec4(color)
    );
    draw_list.ChannelsMerge();
}

void render_windows()
{
    auto lock = std::unique_lock{notifications_mutex()};

    std::erase_if(notifications(), [](NotificationImpl const& notification) {
        return notification.has_expired();
    });

    float height = 0.f;
    for (size_t i = 0; i < notifications().size(); ++i)
    {
        ImVec2 const main_window_pos  = ImGui::GetMainViewport()->Pos;
        ImVec2 const main_window_size = ImGui::GetMainViewport()->Size;

        if (height > main_window_size.y - 100.f)
            break;

        auto& notif = notifications()[i];
        notif.init_creation_time_ifn(); // Init creation time the first time a notification is shown, because if they are outside the window they might prevent it from showing for a while, and we don't want it to disappear immediately after appearing

        float const fade_percent = notif.fade_percent();

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
                float const fade_percent = *reinterpret_cast<float const*>(data->UserData); // NOLINT(*reinterpret-cast)
                data->DesiredSize        = ImVec2{data->DesiredSize.x, data->DesiredSize.y * fade_percent};
            },
            (void*)&fade_percent // NOLINT(*casting)
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

            // Title
            background(get_style().color_title_background, [&]() {
                ImGui::TextColored(notif.color(), "%s", notif.icon());
                ImGui::SameLine();
                ImGui::TextUnformatted(notif.title().c_str());
            });

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
        height += ImGui::GetWindowHeight() + get_style().padding_between_notifications_y * fade_percent;

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
