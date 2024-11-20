#include <ImGuiNotify/ImGuiNotify.hpp>
#include <quick_imgui/quick_imgui.hpp>

auto main(int argc, char* argv[]) -> int
{
    quick_imgui::loop("ImGuiNotify tests", [&]() { // Open a window and run all the ImGui-related code
        ImGui::Begin("ImGuiNotify tests");
        ImGui::End();
        ImGui::ShowDemoWindow();
    });
}
