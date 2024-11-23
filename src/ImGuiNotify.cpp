#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
//
#include <algorithm>
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
    auto unique_id() const -> std::string const& { return _unique_id; }
    auto has_been_init() const -> bool { return _creation_time.has_value(); }

    auto elapsed_time() const
    {
        assert(has_been_init());
        return std::chrono::steady_clock::now() - *_creation_time;
    }

    auto has_expired() const -> bool
    {
        return has_been_init()
               && elapsed_time() > _notification.duration + get_style().fade_in_duration + get_style().fade_out_duration;
    }

    auto is_fading_out() const -> bool
    {
        return has_been_init()
               && elapsed_time() > _notification.duration + get_style().fade_in_duration;
    }
    auto fade_percent() const -> float
    {
        float const elapsed_ms  = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time()).count());
        float const fade_in_ms  = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(get_style().fade_in_duration).count());
        float const fade_out_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(get_style().fade_out_duration).count());
        float const duration_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(_notification.duration).count());

        float opacity = 1.f;

        if (elapsed_ms < fade_in_ms)
            opacity = elapsed_ms / fade_in_ms;
        else if (elapsed_ms > duration_ms + fade_in_ms)
            opacity = 1.f - (elapsed_ms - fade_in_ms - duration_ms) / fade_out_ms;

        return std::clamp(opacity, 0.f, 1.f);
    }

    void init_creation_time_ifn()
    {
        if (_creation_time.has_value())
            return;
        _creation_time.emplace(std::chrono::steady_clock::now());
    }

    void reset_creation_time()
    {
        if (elapsed_time() > get_style().fade_in_duration)
            _creation_time.emplace(std::chrono::steady_clock::now() - get_style().fade_in_duration);
    }

private:
    Notification                                         _notification;
    std::optional<std::chrono::steady_clock::time_point> _creation_time{};

    inline static uint64_t _next_id{0};
    std::string            _unique_id{std::to_string(_next_id++)};
};
} // namespace

static auto notifications() -> std::vector<NotificationImpl>&
{
    static auto instance = std::vector<NotificationImpl>{};
    return instance;
}

void send(Notification notification)
{
    notifications().emplace_back(std::move(notification));
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
    std::erase_if(notifications(), [](NotificationImpl const& notification) {
        return notification.has_expired();
    });

    float height = 0.f;
    for (size_t i = 0; i < std::min(notifications().size(), get_style().render_limit); ++i)
    {
        auto& notif = notifications()[i];

        notif.init_creation_time_ifn(); // Init creation time the first time a notification is shown, because get_style().render_limit might prevent it from showing for a while, and we don't want it to disappear immediately after appearing

        float const fade_percent = notif.fade_percent();

        // Set window position and size
        ImVec2 const main_window_pos  = ImGui::GetMainViewport()->Pos;
        ImVec2 const main_window_size = ImGui::GetMainViewport()->Size;
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
        ImGui::Begin(("##notification" + notif.unique_id()).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);

        // Render over all other windows
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        // Keep alive if hovered
        if (ImGui::IsWindowHovered())
            notif.reset_creation_time();

        // Here we render the content
        {
            ImGui::PushTextWrapPos(main_window_size.x / 3.f); // We want to support multi-line text, this will wrap the text after 1/3 of the screen width

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
