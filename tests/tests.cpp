#include "ImGuiNotify/ImGuiNotify.hpp"
#include "quick_imgui/quick_imgui.hpp"

auto main(int argc, char* argv[]) -> int
{
    bool const should_run_imgui_tests = argc < 2 || strcmp(argv[1], "-nogpu") != 0; // NOLINT(*pointer-arithmetic)
    if (!should_run_imgui_tests)
        return 0;

    quick_imgui::loop(
        "ImGuiNotify tests",
        []() { // Init
            ImGui::GetIO().Fonts->AddFontDefault();
            ImGuiNotify::add_icons_to_current_font();
        },
        [&]() { // Loop
            ImGui::Begin("ImGuiNotify tests");
            if (ImGui::Button("Success"))
                ImGuiNotify::send({.type = ImGuiNotify::Type::Success, .content = "Hello"});
            if (ImGui::Button("Warning"))
                ImGuiNotify::send({.type = ImGuiNotify::Type::Warning, .title = "Warning", .content = "Hello"});
            if (ImGui::Button("Error"))
                ImGuiNotify::send({.type = ImGuiNotify::Type::Error, .content = "HelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHello"});
            if (ImGui::Button("Info"))
                ImGuiNotify::send({.type = ImGuiNotify::Type::Info, .content = "Hello"});
            ImGui::End();

            ImGuiNotify::render_windows();
        }
    );
}
