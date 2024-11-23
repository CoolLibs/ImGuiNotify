cmake_minimum_required(VERSION 3.8)

set(WARNINGS_AS_ERRORS_FOR_IMGUI_NOTIFY OFF CACHE BOOL "ON iff you want to treat warnings as errors")

add_library(ImGuiNotify)
add_library(ImGuiNotify::ImGuiNotify ALIAS ImGuiNotify)
target_compile_features(ImGuiNotify PUBLIC cxx_std_20)

# ---Add source files---
if(WARNINGS_AS_ERRORS_FOR_IMGUI_NOTIFY)
    target_include_directories(ImGuiNotify PUBLIC include)
    target_include_directories(ImGuiNotify PRIVATE font)
else()
    target_include_directories(ImGuiNotify SYSTEM PUBLIC include)
    target_include_directories(ImGuiNotify SYSTEM PRIVATE font)
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
target_sources(ImGuiNotify PRIVATE ${SRC_FILES})

# ---Set warning level---
if(MSVC)
    target_compile_options(ImGuiNotify PRIVATE /W4)
else()
    target_compile_options(ImGuiNotify PRIVATE -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wsign-conversion -Wimplicit-fallthrough)
endif()

# ---Maybe enable warnings as errors---
if(WARNINGS_AS_ERRORS_FOR_IMGUI_NOTIFY)
    if(MSVC)
        target_compile_options(ImGuiNotify PRIVATE /WX)
    else()
        target_compile_options(ImGuiNotify PRIVATE -Werror)
    endif()
endif()

# ---Install license---
install(FILES "LICENSE" DESTINATION "license/ImGuiNotify")