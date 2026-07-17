cmake_minimum_required(VERSION 3.28)

project(
    VidChopper
    VERSION 0.2.0
    DESCRIPTION "Qt desktop app for splitting videos into chapter clips with ffmpeg"
    LANGUAGES CXX
)

option(VIDCHOPPER_BUILD_CLI "Build the no-Qt CLI target" ON)
option(VIDCHOPPER_BUILD_GUI "Build the Qt GUI target" ON)
option(VIDCHOPPER_BUILD_TESTS "Build the test targets" ON)
set(VIDCHOPPER_DISPLAY_VERSION "0.2.0-alpha" CACHE STRING "User-facing version string for prerelease labeling")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(apply_vidchopper_warnings target_name)
    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /W4 /permissive- /EHsc /MP /utf-8 /Zc:__cplusplus)
    else()
        target_compile_options("${target_name}" PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(vidchopper_core
    src/core/chapter_plan.cpp
    src/core/chapter_plan.hpp
    src/core/command_builder.cpp
    src/core/command_builder.hpp
    src/core/enum_utils.hpp
    src/core/models.hpp
    src/core/string_utils.cpp
    src/core/string_utils.hpp
    src/core/timecode.cpp
    src/core/timecode.hpp
    src/core/types.hpp
)

target_include_directories(vidchopper_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(vidchopper_core PUBLIC cxx_std_20)
apply_vidchopper_warnings(vidchopper_core)

if(VIDCHOPPER_BUILD_CLI)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(yaml-cpp CONFIG REQUIRED)

    add_library(vidchopper_cli
        src/cli/chapter_config.cpp
        src/cli/chapter_config.hpp
        src/cli/cli_app.cpp
        src/cli/cli_app.hpp
        src/cli/cli_arguments.cpp
        src/cli/cli_arguments.hpp
        src/cli/cli_settings.cpp
        src/cli/cli_settings.hpp
    )

    target_include_directories(vidchopper_cli PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(vidchopper_cli PUBLIC
        vidchopper_core
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
    )
    target_compile_features(vidchopper_cli PUBLIC cxx_std_20)
    apply_vidchopper_warnings(vidchopper_cli)

    add_executable(VidChopperCLI src/cli/main.cpp)
    target_link_libraries(VidChopperCLI PRIVATE vidchopper_cli)
    apply_vidchopper_warnings(VidChopperCLI)
    set_target_properties(VidChopperCLI PROPERTIES OUTPUT_NAME "VidChopperCLI")
endif()

include(CTest)

if(VIDCHOPPER_BUILD_TESTS)
    enable_testing()

    function(add_vidchopper_test target_name source_file label_name)
        add_executable("${target_name}" "${source_file}")
        target_link_libraries("${target_name}" PRIVATE vidchopper_core ${ARGN})
        target_include_directories("${target_name}" PRIVATE "${PROJECT_SOURCE_DIR}/tests")
        apply_vidchopper_warnings("${target_name}")

        add_test(NAME "${label_name}.${target_name}" COMMAND "${target_name}")
        set_tests_properties("${label_name}.${target_name}" PROPERTIES LABELS "${label_name}")
    endfunction()

    add_vidchopper_test(vidchopper_test_timecode tests/test_timecode.cpp fast)
    add_vidchopper_test(vidchopper_test_timecode_extended tests/test_timecode_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_format_parse_golden tests/test_format_parse_golden.cpp fast)
    add_vidchopper_test(vidchopper_test_chapter_plan tests/test_chapter_plan.cpp fast)
    add_vidchopper_test(vidchopper_test_chapter_plan_extended tests/test_chapter_plan_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_chapter_plan_golden tests/test_chapter_plan_golden.cpp fast)
    add_vidchopper_test(vidchopper_test_command_builder tests/test_command_builder.cpp fast)
    add_vidchopper_test(vidchopper_test_command_builder_extended tests/test_command_builder_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_command_builder_golden tests/test_command_builder_golden.cpp fast)
    add_vidchopper_test(vidchopper_test_string_utils tests/test_string_utils.cpp fast)
    add_vidchopper_test(vidchopper_test_string_utils_extended tests/test_string_utils_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_models tests/test_models.cpp fast)
    add_vidchopper_test(vidchopper_test_enum_clamp tests/test_enum_clamp.cpp fast)
    add_vidchopper_test(vidchopper_test_demo_launch_options tests/test_demo_launch_options.cpp fast)
    if(TARGET vidchopper_cli)
        add_vidchopper_test(vidchopper_test_cli_arguments tests/test_cli_arguments.cpp fast vidchopper_cli)
        add_vidchopper_test(vidchopper_test_cli_settings tests/test_cli_settings.cpp fast vidchopper_cli)
        add_vidchopper_test(vidchopper_test_cli_contract tests/test_cli_contract.cpp fast vidchopper_cli)
        add_vidchopper_test(vidchopper_test_chapter_config tests/test_chapter_config.cpp fast vidchopper_cli)
    endif()
    add_vidchopper_test(vidchopper_test_ffmpeg_integration tests/test_ffmpeg_integration.cpp slow)
endif()

if(VIDCHOPPER_BUILD_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets)

    qt_standard_project_setup()

    set(VIDCHOPPER_WIN_RESOURCE "")
    if(WIN32)
        set(VIDCHOPPER_WIN_RESOURCE resources/app_icon.rc)
    endif()

    add_executable(VidChopper
        src/qt/main.cpp
        src/qt/app_settings.cpp
        src/qt/app_settings.hpp
        src/qt/demo_launch_options.hpp
        src/qt/dark_palette.cpp
        src/qt/dark_palette.hpp
        src/qt/services/export_coordinator.cpp
        src/qt/services/export_coordinator.hpp
        src/qt/services/ffprobe_service.cpp
        src/qt/services/ffprobe_service.hpp
        src/qt/services/gpu_detector.cpp
        src/qt/services/gpu_detector.hpp
        src/qt/ui/advanced_settings_dialog.cpp
        src/qt/ui/advanced_settings_dialog.hpp
        src/qt/ui/chapter_table_model.cpp
        src/qt/ui/chapter_table_model.hpp
        src/qt/ui/main_window.cpp
        src/qt/ui/main_window.hpp
        resources/app_icon.qrc
        ${VIDCHOPPER_WIN_RESOURCE}
    )

    target_link_libraries(VidChopper PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets vidchopper_core)
    target_compile_definitions(VidChopper PRIVATE VIDCHOPPER_DISPLAY_VERSION="${VIDCHOPPER_DISPLAY_VERSION}")
    apply_vidchopper_warnings(VidChopper)

    set_target_properties(VidChopper PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME "VidChopper")
endif()
