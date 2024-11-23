#include <ImGuiNotify/ImGuiNotify.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "IconsFontAwesome6.h"
#include "fa-solid-900.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace ImGui::Notify {

static constexpr float NOTIFY_PADDING_X         = 20.f;       // Bottom-left X padding
static constexpr float NOTIFY_PADDING_Y         = 1.f * 20.f; // Bottom-left Y padding
static constexpr float NOTIFY_PADDING_MESSAGE_Y = 1.f * 10.f; // Padding Y between each message
static constexpr float NOTIFY_MIN_WIDTH         = 275.f;
#define NOTIFY_FADE_IN_OUT_TIME 200             // Fade in and out duration
static constexpr size_t NOTIFY_RENDER_LIMIT{5}; // Max number of toasts rendered at the same time. Set to 0 for unlimited

#define NOTIFY_NULL_OR_EMPTY(str) (!str || !strlen(str))

static const ImGuiWindowFlags NOTIFY_DEFAULT_TOAST_FLAGS = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;

class ToastImpl {
public:
    auto getDefaultTitle() const -> std::string_view
    {
        if (!_toast.title.empty())
            return _toast.title;

        switch (_toast.type)
        {
        case ToastType::None:
            return "";
        case ToastType::Success:
            return "Success";
        case ToastType::Warning:
            return "Warning";
        case ToastType::Error:
            return "Error";
        case ToastType::Info:
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
        // TODO(Toast) use nicer colors

        switch (_toast.type)
        {
        case ToastType::None:
            return {1.f, 1.f, 1.f, 1.f}; // White
        case ToastType::Success:
            return style().success;
        case ToastType::Warning:
            return style().warning;
        case ToastType::Error:
            return style().error;
        case ToastType::Info:
            return style().info;
        default:
            assert(false);
            return {1.f, 1.f, 1.f, 1.f}; // White
        }
    }

    auto getIcon() const -> const char*
    {
        switch (_toast.type)
        {
        case ToastType::None:
            return nullptr;
        case ToastType::Success:
            return ICON_FA_CIRCLE_CHECK; // Font Awesome 6
        case ToastType::Warning:
            return ICON_FA_TRIANGLE_EXCLAMATION; // Font Awesome 6
        case ToastType::Error:
            return ICON_FA_CIRCLE_EXCLAMATION; // Font Awesome 6
        case ToastType::Info:
            return ICON_FA_CIRCLE_INFO; // Font Awesome 6
        default:
            assert(false);
            return nullptr;
        }
    }

    auto getElapsedTimeMS() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - creation_time()).count();
    }

    auto has_been_init() const -> bool
    {
        return _creationTime.has_value();
    }
    auto has_expired() const -> bool
    {
        return has_been_init() && getElapsedTimeMS() > _toast.dismissTime + 2 * NOTIFY_FADE_IN_OUT_TIME;
    }

    auto creation_time() const -> std::chrono::steady_clock::time_point { return *_creationTime; }

    void init_creation_time_ifn()
    {
        if (_creationTime.has_value())
            return;
        _creationTime.emplace(std::chrono::steady_clock::now());
    }

    void reset_creation_time()
    {
        _creationTime.emplace(std::chrono::steady_clock::now() - std::chrono::milliseconds{NOTIFY_FADE_IN_OUT_TIME});
    }

    auto is_fading_out() const -> bool
    {
        return has_been_init() && getElapsedTimeMS() > _toast.dismissTime + NOTIFY_FADE_IN_OUT_TIME;
    }

    auto getFadePercent() const -> float
    {
        auto const elapsed = getElapsedTimeMS();

        float opacity = 1.f;

        if (elapsed < NOTIFY_FADE_IN_OUT_TIME)
        {
            opacity = (float)elapsed / (float)NOTIFY_FADE_IN_OUT_TIME;
        }
        else if (elapsed > _toast.dismissTime + NOTIFY_FADE_IN_OUT_TIME)
        {
            opacity = 1.f - (((float)elapsed - (float)NOTIFY_FADE_IN_OUT_TIME - (float)_toast.dismissTime) / (float)NOTIFY_FADE_IN_OUT_TIME);
        }

        return std::clamp(opacity, 0.f, 1.f);
    }

public:
    Toast                                                _toast;
    std::optional<std::chrono::steady_clock::time_point> _creationTime{};
    float                                                _window_height{};
    inline static uint64_t                               next_id{0};
    std::string                                          _uniqueId{std::to_string(next_id++)};
    ImGuiWindowFlags                                     _flags = NOTIFY_DEFAULT_TOAST_FLAGS;
};

inline std::vector<ToastImpl> notifications;

void add(Toast toast)
{
    notifications.push_back(ToastImpl{std::move(toast)});
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
    std::erase_if(notifications, [](ToastImpl const& toast) {
        return toast.has_expired();
    });
    const ImVec2 mainWindowSize = GetMainViewport()->Size;

    float height = 0.f;
    // if (!notifications.empty() && notifications[0].is_fading_out())
    //     height -= (1.f - notifications[0].getFadePercent()) * (notifications[0]._window_height + NOTIFY_PADDING_MESSAGE_Y);

    for (size_t i = 0; i < std::min(notifications.size(), NOTIFY_RENDER_LIMIT); ++i)
    {
        ToastImpl& currentToast = notifications[i];

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
        ImVec2 mainWindowPos = GetMainViewport()->Pos;
        SetNextWindowPos(ImVec2(mainWindowPos.x + mainWindowSize.x - NOTIFY_PADDING_X, mainWindowPos.y + mainWindowSize.y - NOTIFY_PADDING_Y - height), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowSizeConstraints(
            {NOTIFY_MIN_WIDTH, 0.f}, {FLT_MAX, FLT_MAX}, [](ImGuiSizeCallbackData* data) {
                data->DesiredSize = {data->DesiredSize.x, data->DesiredSize.y * (*(float*)data->UserData)};
            },
            (void*)&opacity
        );
        // Set notification window flags
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5.f);
        ImVec4 textColor = currentToast.getColor();
        ImGui::PushStyleColor(ImGuiCol_Border, textColor); // Red color
        // textColor.w = opacity;

        Begin(windowName.c_str(), nullptr, currentToast._flags);

        // Render over all other windows
        BringWindowToDisplayFront(GetCurrentWindow());

        if (ImGui::IsWindowHovered())
            currentToast.reset_creation_time();

        // Here we render the toast content
        {
            PushTextWrapPos(mainWindowSize.x / 3.f); // We want to support multi-line text, this will wrap the text after 1/3 of the screen width

            bool wasTitleRendered = false;

            background(style().title_background, [&]() {
                // If an icon is set
                if (!NOTIFY_NULL_OR_EMPTY(icon))
                {
                    // Text(icon); // Render icon text
                    TextColored(textColor, "%s", icon);
                    wasTitleRendered = true;
                }

                // If a title is set
                if (!NOTIFY_NULL_OR_EMPTY(title))
                {
                    // If a title and an icon is set, we want to render on same line
                    if (!NOTIFY_NULL_OR_EMPTY(icon))
                        SameLine();

                    Text("%s", title); // Render title text
                    wasTitleRendered = true;
                }
                else if (!NOTIFY_NULL_OR_EMPTY(defaultTitle))
                {
                    if (!NOTIFY_NULL_OR_EMPTY(icon))
                        SameLine();

                    Text("%s", defaultTitle); // Render default title text (ImGuiToastType_Success -> "Success", etc...)
                    wasTitleRendered = true;
                }
            });

            // In case ANYTHING was rendered in the top, we want to add a small padding so the text (or icon) looks centered vertically
            if (wasTitleRendered && (!NOTIFY_NULL_OR_EMPTY(content) || currentToast._toast.custom_imgui_content))
            {
                SetCursorPosY(GetCursorPosY() + 5.f); // Must be a better way to do this!!!!
            }

            // If a content is set
            if (!NOTIFY_NULL_OR_EMPTY(content))
            {
                Text("%s", content); // Render content text
            }

            if (currentToast._toast.custom_imgui_content)
                currentToast._toast.custom_imgui_content();

            PopTextWrapPos();
        }

        // Save height for next toasts
        currentToast._window_height = GetWindowHeight();
        height += currentToast._window_height + NOTIFY_PADDING_MESSAGE_Y * opacity;

        // End
        End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
}

void add_icons_to_current_font(float icons_size)
{
    static constexpr ImWchar iconsRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
    ImFontConfig             iconsConfig;
    iconsConfig.MergeMode        = true;
    iconsConfig.PixelSnapH       = true;
    iconsConfig.GlyphMinAdvanceX = icons_size;
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size, icons_size, &iconsConfig, iconsRanges);
}

} // namespace ImGui::Notify
