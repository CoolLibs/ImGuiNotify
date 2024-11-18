/**
 * @file ImGuiNotify.hpp
 * @brief A header-only library for creating toast notifications with ImGui.
 *
 * Based on imgui-notify by patrickcjk
 * https://github.com/patrickcjk/imgui-notify
 *
 * @version 0.0.3 by TyomaVader
 * @date 07.07.2024
 */

#pragma once
#include <chrono>     // For the notifications timed dissmiss
#include <functional> // For storing the code, which executest on the button click in the notification
#include <string>
#include <string_view>
#include <vector> // Vector for storing notifications list
#include "IconsFontAwesome6.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace ImGui {

/**
 * CONFIGURATION SECTION Start
 */

#define NOTIFY_PADDING_X          20.f  // Bottom-left X padding
#define NOTIFY_PADDING_Y          20.f  // Bottom-left Y padding
#define NOTIFY_PADDING_MESSAGE_Y  10.f  // Padding Y between each message
#define NOTIFY_FADE_IN_OUT_TIME   150   // Fade in and out duration
#define NOTIFY_DEFAULT_DISMISS    3000  // Auto dismiss after X ms (default, applied only of no data provided in constructors)
#define NOTIFY_OPACITY            0.8f  // 0-1 Toast opacity
#define NOTIFY_USE_SEPARATOR      false // If true, a separator will be rendered between the title and the content
#define NOTIFY_USE_DISMISS_BUTTON true  // If true, a dismiss button will be rendered in the top right corner of the toast
#define NOTIFY_RENDER_LIMIT       5     // Max number of toasts rendered at the same time. Set to 0 for unlimited
// Warning: Requires ImGui docking with multi-viewport enabled
#define NOTIFY_RENDER_OUTSIDE_MAIN_WINDOW false // If true, the notifications will be rendered in the corner of the monitor, otherwise in the corner of the main window

/**
 * CONFIGURATION SECTION End
 */

static const ImGuiWindowFlags NOTIFY_DEFAULT_TOAST_FLAGS = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;

#define NOTIFY_NULL_OR_EMPTY(str) (!str || !strlen(str))

enum class ToastType : uint8_t {
    None,
    Success,
    Warning,
    Error,
    Info,
    COUNT
};

enum class ToastPhase : uint8_t {
    FadeIn,
    Wait,
    FadeOut,
    Expired,
    COUNT
};

enum class ToastPos : uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    Center,
    COUNT
};

struct Toast {
    ImGuiWindowFlags flags = NOTIFY_DEFAULT_TOAST_FLAGS;

    ToastType   type{ToastType::None};
    std::string title{""};
    std::string content{""};

    int                   dismissTime   = NOTIFY_DEFAULT_DISMISS;
    std::function<void()> onButtonPress = nullptr; // A lambda variable, which will be executed when button in notification is pressed
};

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
        switch (_toast.type)
        {
        case ToastType::None:
            return {255, 255, 255, 255}; // White
        case ToastType::Success:
            return {0, 255, 0, 255}; // Green
        case ToastType::Warning:
            return {255, 255, 0, 255}; // Yellow
        case ToastType::Error:
            return {255, 0, 0, 255}; // Red
        case ToastType::Info:
            return {0, 157, 255, 255}; // Blue
        default:
            assert(false);
            return {255, 255, 255, 255}; // White
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

    /**
     * @brief Get the elapsed time in milliseconds since the creation of the object.
     *
     * @return int64_t The elapsed time in milliseconds.
     * @throws An exception with the message "Unsupported platform" if the platform is not supported.
     */
    auto getElapsedTime() const -> std::chrono::nanoseconds
    {
        return std::chrono::steady_clock::now() - _creationTime;
    }

    /**
     * @brief Get the current phase of the toast notification based on the elapsed time since its creation.
     *
     * @return ImGuiToastPhase The current phase of the toast notification.
     *         - ImGuiToastPhase::FadeIn: The notification is fading in.
     *         - ImGuiToastPhase::Wait: The notification is waiting to be dismissed.
     *         - ImGuiToastPhase::FadeOut: The notification is fading out.
     *         - ImGuiToastPhase::Expired: The notification has expired and should be removed.
     */
    auto getPhase() const -> ToastPhase
    {
        const int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getElapsedTime()).count();

        if (elapsed > NOTIFY_FADE_IN_OUT_TIME + _toast.dismissTime + NOTIFY_FADE_IN_OUT_TIME)
        {
            return ToastPhase::Expired;
        }
        else if (elapsed > NOTIFY_FADE_IN_OUT_TIME + _toast.dismissTime)
        {
            return ToastPhase::FadeOut;
        }
        else if (elapsed > NOTIFY_FADE_IN_OUT_TIME)
        {
            return ToastPhase::Wait;
        }
        else
        {
            return ToastPhase::FadeIn;
        }
    }

    /**
     * Returns the percentage of fade for the notification.
     * @return The percentage of fade for the notification.
     */
    auto getFadePercent() const -> float
    {
        const ToastPhase phase   = getPhase();
        const int64_t    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getElapsedTime()).count();

        if (phase == ToastPhase::FadeIn)
        {
            return ((float)elapsed / (float)NOTIFY_FADE_IN_OUT_TIME) * NOTIFY_OPACITY;
        }
        else if (phase == ToastPhase::FadeOut)
        {
            return (1.f - (((float)elapsed - (float)NOTIFY_FADE_IN_OUT_TIME - (float)_toast.dismissTime) / (float)NOTIFY_FADE_IN_OUT_TIME)) * NOTIFY_OPACITY;
        }

        return NOTIFY_OPACITY;
    }

public:
    Toast                                 _toast;
    std::chrono::steady_clock::time_point _creationTime = std::chrono::steady_clock::now();
};

inline std::vector<ToastImpl> notifications;

/**
 * Inserts a new notification into the notification queue.
 * @param toast The notification to be inserted.
 */
inline void InsertNotification(Toast const& toast)
{
    notifications.push_back(ToastImpl{toast});
}

/**
 * @brief Removes a notification from the list of notifications.
 *
 * @param index The index of the notification to remove.
 */
inline void RemoveNotification(int index)
{
    notifications.erase(notifications.begin() + index);
}

/**
 * Renders all notifications in the notifications vector.
 * Each notification is rendered as a toast window with a title, content and an optional icon.
 * If a notification is expired, it is removed from the vector.
 */
inline void RenderNotifications()
{
    const ImVec2 mainWindowSize = GetMainViewport()->Size;

    float height = 0.f;

    for (size_t i = 0; i < notifications.size(); ++i)
    {
        ToastImpl const& currentToast = notifications[i];

        // Remove toast if expired
        if (currentToast.getPhase() == ToastPhase::Expired)
        {
            RemoveNotification(i);
            continue;
        }

#if NOTIFY_RENDER_LIMIT > 0
        if (i > NOTIFY_RENDER_LIMIT)
        {
            continue;
        }
#endif

        // Get icon, title and other data
        const char* icon         = currentToast.getIcon();
        const char* title        = currentToast._toast.title.c_str();
        const char* content      = currentToast._toast.content.c_str();
        const char* defaultTitle = currentToast.getDefaultTitle().data();
        const float opacity      = currentToast.getFadePercent(); // Get opacity based of the current phase

        // Window rendering
        ImVec4 textColor = currentToast.getColor();
        textColor.w      = opacity;

        // Generate new unique name for this toast
        char windowName[50];
        // TODO(Toast)
#ifdef _WIN32
        sprintf_s(windowName, "##TOAST%d", (int)i);
#elif defined(__linux__) || defined(__EMSCRIPTEN__)
        std::sprintf(windowName, "##TOAST%d", (int)i);
#elif defined(__APPLE__)
        std::snprintf(windowName, 50, "##TOAST%d", (int)i);
#else
        throw "Unsupported platform";
#endif

        // PushStyleColor(ImGuiCol_Text, textColor);
        SetNextWindowBgAlpha(opacity);

#if NOTIFY_RENDER_OUTSIDE_MAIN_WINDOW
        short mainMonitorId = static_cast<ImGuiViewportP*>(GetMainViewport())->PlatformMonitor;

        ImGuiPlatformIO&      platformIO = GetPlatformIO();
        ImGuiPlatformMonitor& monitor    = platformIO.Monitors[mainMonitorId];

        // Set notification window position to bottom right corner of the monitor
        SetNextWindowPos(ImVec2(monitor.WorkPos.x + monitor.WorkSize.x - NOTIFY_PADDING_X, monitor.WorkPos.y + monitor.WorkSize.y - NOTIFY_PADDING_Y - height), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
#else
        // Set notification window position to bottom right corner of the main window, considering the main window size and location in relation to the display
        ImVec2 mainWindowPos = GetMainViewport()->Pos;
        SetNextWindowPos(ImVec2(mainWindowPos.x + mainWindowSize.x - NOTIFY_PADDING_X, mainWindowPos.y + mainWindowSize.y - NOTIFY_PADDING_Y - height), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
#endif

        // Set notification window flags
        // if (!NOTIFY_USE_DISMISS_BUTTON && currentToast._toast.onButtonPress == nullptr)
        // {
        //     currentToast.setWindowFlags(NOTIFY_DEFAULT_TOAST_FLAGS | ImGuiWindowFlags_NoInputs);
        // }

        Begin(windowName, nullptr, currentToast._toast.flags);

        // Render over all other windows
        BringWindowToDisplayFront(GetCurrentWindow());

        // Here we render the toast content
        {
            PushTextWrapPos(mainWindowSize.x / 3.f); // We want to support multi-line text, this will wrap the text after 1/3 of the screen width

            bool wasTitleRendered = false;

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

            // If a dismiss button is enabled
            if (NOTIFY_USE_DISMISS_BUTTON)
            {
                // If a title or content is set, we want to render the button on the same line
                if (wasTitleRendered || !NOTIFY_NULL_OR_EMPTY(content))
                {
                    SameLine();
                }

                // Render the dismiss button on the top right corner
                // NEEDS TO BE REWORKED
                float scale = 0.8f;

                if (CalcTextSize(content).x > GetContentRegionAvail().x)
                {
                    scale = 0.8f;
                }

                SetCursorPosX(GetCursorPosX() + (GetWindowSize().x - GetCursorPosX()) * scale);

                // If the button is pressed, we want to remove the notification
                if (Button(ICON_FA_XMARK))
                {
                    RemoveNotification(i);
                }
            }

            // In case ANYTHING was rendered in the top, we want to add a small padding so the text (or icon) looks centered vertically
            if (wasTitleRendered && !NOTIFY_NULL_OR_EMPTY(content))
            {
                SetCursorPosY(GetCursorPosY() + 5.f); // Must be a better way to do this!!!!
            }

            // If a content is set
            if (!NOTIFY_NULL_OR_EMPTY(content))
            {
                if (wasTitleRendered)
                {
#if NOTIFY_USE_SEPARATOR
                    Separator();
#endif
                }

                Text("%s", content); // Render content text
            }

            // If a button is set
            // TODO(Toast) render custom content
            // if (currentToast.getOnButtonPress() != nullptr)
            // {
            //     // If the button is pressed, we want to execute the lambda function
            //     if (Button(currentToast.getButtonLabel()))
            //     {
            //         currentToast.getOnButtonPress()();
            //     }
            // }

            PopTextWrapPos();
        }

        // Save height for next toasts
        height += GetWindowHeight() + NOTIFY_PADDING_MESSAGE_Y;

        // End
        End();
    }
}
} // namespace ImGui
