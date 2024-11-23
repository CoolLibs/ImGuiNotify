#include "ImGuiNotify/ImGuiNotify.hpp"
#include <algorithm>
#include <optional>
#include <vector>
#include "IconsFontAwesome6.h"
#include "fa-solid-900.h"
#include "imgui_internal.h"

namespace ImGuiNotify {

#define NOTIFY_NULL_OR_EMPTY(str) (!str || !strlen(str))

namespace {
class NotificationImpl {
public:
    auto getDefaultTitle() const -> std::string_view
    {
        if (!_toast.title.empty())
            return _toast.title;

        switch (_toast.type)
        {
        case Type::Success:
            return "Success";
        case Type::Warning:
            return "Warning";
        case Type::Error:
            return "Error";
        case Type::Info:
            return "Info";
        default:
        {
            assert(false);
            return "";
        }
        }
    }

    auto getColor() const -> ImVec4
    {
        switch (_toast.type)
        {
        case Type::Success:
            return get_style().success;
        case Type::Warning:
            return get_style().warning;
        case Type::Error:
            return get_style().error;
        case Type::Info:
            return get_style().info;
        default:
            assert(false);
            return {1.f, 1.f, 1.f, 1.f};
        }
    }

    auto getIcon() const -> const char*
    {
        switch (_toast.type)
        {
        case Type::Success:
            return ICON_FA_CIRCLE_CHECK; // Font Awesome 6
        case Type::Warning:
            return ICON_FA_TRIANGLE_EXCLAMATION; // Font Awesome 6
        case Type::Error:
            return ICON_FA_CIRCLE_EXCLAMATION; // Font Awesome 6
        case Type::Info:
            return ICON_FA_CIRCLE_INFO; // Font Awesome 6
        default:
            assert(false);
            return nullptr;
        }
    }

    auto getElapsedTime() const
    {
        return std::chrono::steady_clock::now() - *_creationTime;
    }

    auto has_been_init() const -> bool
    {
        return _creationTime.has_value();
    }
    auto has_expired() const -> bool
    {
        return has_been_init()
               && getElapsedTime() > _toast.duration + get_style().fade_in_duration + get_style().fade_out_duration;
    }

    void init_creation_time_ifn()
    {
        if (_creationTime.has_value())
            return;
        _creationTime.emplace(std::chrono::steady_clock::now());
    }

    void reset_creation_time()
    {
        if (getElapsedTime() > get_style().fade_in_duration)
            _creationTime.emplace(std::chrono::steady_clock::now() - get_style().fade_in_duration);
    }

    auto is_fading_out() const -> bool
    {
        return has_been_init() && getElapsedTime() > _toast.duration + get_style().fade_in_duration;
    }

    auto getFadePercent() const -> float
    {
        float const elapsed_ms  = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(getElapsedTime()).count());
        float const fade_in_ms  = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(get_style().fade_in_duration).count());
        float const fade_out_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(get_style().fade_out_duration).count());
        float const duration_ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(_toast.duration).count());

        float opacity = 1.f;

        if (elapsed_ms < fade_in_ms)
            opacity = elapsed_ms / fade_in_ms;
        else if (elapsed_ms > duration_ms + fade_in_ms)
            opacity = 1.f - (elapsed_ms - fade_in_ms - duration_ms) / fade_out_ms;

        return std::clamp(opacity, 0.f, 1.f);
    }

public:
    Notification                                         _toast;
    std::optional<std::chrono::steady_clock::time_point> _creationTime{};
    float                                                _window_height{};
    inline static uint64_t                               next_id{0};
    std::string                                          _uniqueId{std::to_string(next_id++)};
};
} // namespace

static auto notifications() -> std::vector<NotificationImpl>&
{
    static auto instance = std::vector<NotificationImpl>{};
    return instance;
}

void send(Notification notification)
{
    notifications().push_back(NotificationImpl{std::move(notification)});
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
        rectangle_start_pos,
        rectangle_end_pos,
        ImU32_from_ImVec4(color)
    );
    draw_list.ChannelsMerge();
}

void render_windows()
{
    std::erase_if(notifications(), [](NotificationImpl const& toast) {
        return toast.has_expired();
    });
    ImVec2 const mainWindowSize = ImGui::GetMainViewport()->Size;

    float height = 0.f;

    for (size_t i = 0; i < std::min(notifications().size(), get_style().render_limit); ++i)
    {
        NotificationImpl& currentToast = notifications()[i];

        // Init creation time the first time a toast is shown, because NOTIFY_RENDER_LIMIT might prevent it from showing for a while, and we don't want it to disappear immediately after appearing
        currentToast.init_creation_time_ifn();

        // Get icon, title and other data
        const char* icon         = currentToast.getIcon();
        const char* title        = currentToast._toast.title.c_str();
        const char* content      = currentToast._toast.content.c_str();
        const char* defaultTitle = currentToast.getDefaultTitle().data();
        const float opacity      = currentToast.getFadePercent();

        // Generate new unique name for this toast
        auto windowName = "##TOAST" + currentToast._uniqueId;

        // Set notification window position to bottom right corner of the main window, considering the main window size and location in relation to the display
        ImVec2 mainWindowPos = ImGui::GetMainViewport()->Pos;
        ImGui::SetNextWindowPos(ImVec2(mainWindowPos.x + mainWindowSize.x - get_style().padding_x, mainWindowPos.y + mainWindowSize.y - get_style().padding_y - height), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowSizeConstraints(
            {get_style().min_width, 0.f}, {FLT_MAX, FLT_MAX}, [](ImGuiSizeCallbackData* data) {
                data->DesiredSize = {data->DesiredSize.x, data->DesiredSize.y * (*(float*)data->UserData)};
            },
            (void*)&opacity
        );
        // Set notification window flags
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, get_style().border_width);
        ImVec4 textColor = currentToast.getColor();
        ImGui::PushStyleColor(ImGuiCol_Border, textColor); // Red color
                                                           // textColor.w = opacity;

        ImGui::Begin(windowName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);

        // Render over all other windows
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        if (ImGui::IsWindowHovered())
            currentToast.reset_creation_time();

        // Here we render the toast content
        {
            ImGui::PushTextWrapPos(mainWindowSize.x / 3.f); // We want to support multi-line text, this will wrap the text after 1/3 of the screen width

            bool wasTitleRendered = false;

            background(get_style().title_background, [&]() {
                // If an icon is set
                if (!NOTIFY_NULL_OR_EMPTY(icon))
                {
                    // Text(icon); // Render icon text
                    ImGui::TextColored(textColor, "%s", icon);
                    wasTitleRendered = true;
                }

                // If a title is set
                if (!NOTIFY_NULL_OR_EMPTY(title))
                {
                    // If a title and an icon is set, we want to render on same line
                    if (!NOTIFY_NULL_OR_EMPTY(icon))
                        ImGui::SameLine();

                    ImGui::Text("%s", title); // Render title text
                    wasTitleRendered = true;
                }
                else if (!NOTIFY_NULL_OR_EMPTY(defaultTitle))
                {
                    if (!NOTIFY_NULL_OR_EMPTY(icon))
                        ImGui::SameLine();

                    ImGui::Text("%s", defaultTitle);
                    wasTitleRendered = true;
                }
            });

            // In case ANYTHING was rendered in the top, we want to add a small padding so the text (or icon) looks centered vertically
            if (wasTitleRendered && (!NOTIFY_NULL_OR_EMPTY(content) || currentToast._toast.custom_imgui_content))
            {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f); // Must be a better way to do this!!!!
            }

            // If a content is set
            if (!NOTIFY_NULL_OR_EMPTY(content))
            {
                ImGui::Text("%s", content); // Render content text
            }

            if (currentToast._toast.custom_imgui_content)
                currentToast._toast.custom_imgui_content();

            ImGui::PopTextWrapPos();
        }

        // Save height for next toasts
        currentToast._window_height = ImGui::GetWindowHeight();
        height += currentToast._window_height + get_style().padding_between_notifications_y * opacity;

        // End
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
}

void add_icons_to_current_font(float icons_size)
{
    static constexpr ImWchar iconsRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0}; // NOLINT(*avoid-c-arrays)
    ImFontConfig             iconsConfig{};
    iconsConfig.MergeMode        = true;
    iconsConfig.PixelSnapH       = true;
    iconsConfig.GlyphMinAdvanceX = icons_size;
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size, icons_size, &iconsConfig, iconsRanges);
}

} // namespace ImGuiNotify
