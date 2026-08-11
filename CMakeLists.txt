cmake_minimum_required(VERSION 3.28)

project(
    VidChopper
    VERSION 1.1.0
    DESCRIPTION "Qt desktop app for splitting videos into chapter clips with ffmpeg"
    LANGUAGES CXX
)

option(VIDCHOPPER_BUILD_CLI "Build the no-Qt CLI target" ON)
option(VIDCHOPPER_BUILD_GUI "Build the Qt GUI target" ON)
option(VIDCHOPPER_BUILD_TESTS "Build the test targets" ON)
option(VIDCHOPPER_BUILD_QT_TESTS "Build Qt-dependent test targets" OFF)
set(VIDCHOPPER_DISPLAY_VERSION "1.1.0" CACHE STRING "User-facing version string")

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

function(apply_vidchopper_display_version target_name)
    target_compile_definitions("${target_name}" PRIVATE VIDCHOPPER_DISPLAY_VERSION="${VIDCHOPPER_DISPLAY_VERSION}")
endfunction()

add_library(vidchopper_core
    src/core/chapter_plan.cpp
    src/core/chapter_plan.hpp
    src/core/command_builder.cpp
    src/core/command_builder.hpp
    src/core/encoder_model.cpp
    src/core/encoder_model.hpp
    src/core/enum_utils.hpp
    src/core/models.hpp
    src/core/ready_marker.cpp
    src/core/ready_marker.hpp
    src/core/string_utils.cpp
    src/core/string_utils.hpp
    src/core/timecode.cpp
    src/core/timecode.hpp
    src/core/types.hpp
)

target_include_directories(vidchopper_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
apply_vidchopper_display_version(vidchopper_core)
target_compile_features(vidchopper_core PUBLIC cxx_std_20)
apply_vidchopper_warnings(vidchopper_core)

find_package(nlohmann_json CONFIG REQUIRED)

add_library(vidchopper_services
    src/services/encoder_capability.cpp
    src/services/encoder_capability.hpp
    src/services/export_engine.cpp
    src/services/export_engine.hpp
    src/services/export_planner.cpp
    src/services/export_planner.hpp
    src/services/manifest_writer.cpp
    src/services/manifest_writer.hpp
    src/services/probe_service.cpp
    src/services/probe_service.hpp
    src/services/process_runner.cpp
    src/services/process_runner.hpp
)

target_include_directories(vidchopper_services PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(vidchopper_services PUBLIC vidchopper_core PRIVATE nlohmann_json::nlohmann_json)
apply_vidchopper_display_version(vidchopper_services)
target_compile_features(vidchopper_services PUBLIC cxx_std_20)
apply_vidchopper_warnings(vidchopper_services)

if(VIDCHOPPER_BUILD_CLI)
    find_package(yaml-cpp CONFIG REQUIRED)

    add_library(vidchopper_cli
        src/cli/batch_resolver.cpp
        src/cli/batch_resolver.hpp
        src/cli/chapter_config.cpp
        src/cli/chapter_config.hpp
        src/cli/chapter_source_policy.cpp
        src/cli/chapter_source_policy.hpp
        src/cli/cli_app.cpp
        src/cli/cli_app.hpp
        src/cli/cli_arguments.cpp
        src/cli/cli_arguments.hpp
        src/cli/cli_settings.cpp
        src/cli/cli_settings.hpp
        src/cli/command_display.cpp
        src/cli/command_display.hpp
        src/cli/dry_run_renderer.cpp
        src/cli/dry_run_renderer.hpp
    )

    target_include_directories(vidchopper_cli PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(vidchopper_cli PUBLIC
        vidchopper_core
        vidchopper_services
        yaml-cpp::yaml-cpp
    )
    target_link_libraries(vidchopper_cli PRIVATE nlohmann_json::nlohmann_json)
    apply_vidchopper_display_version(vidchopper_cli)
    target_compile_features(vidchopper_cli PUBLIC cxx_std_20)
    apply_vidchopper_warnings(vidchopper_cli)

    add_executable(VidChopperCLI src/cli/main.cpp)
    target_link_libraries(VidChopperCLI PRIVATE vidchopper_cli)
    apply_vidchopper_display_version(VidChopperCLI)
    apply_vidchopper_warnings(VidChopperCLI)
    set_target_properties(VidChopperCLI PROPERTIES OUTPUT_NAME "VidChopperCLI")
endif()

include(CTest)

if(VIDCHOPPER_BUILD_TESTS OR VIDCHOPPER_BUILD_QT_TESTS)
    enable_testing()
endif()

if(VIDCHOPPER_BUILD_TESTS)
    function(add_vidchopper_test target_name source_file label_name)
        add_executable("${target_name}" "${source_file}")
        target_link_libraries("${target_name}" PRIVATE vidchopper_core ${ARGN})
        target_include_directories("${target_name}" PRIVATE "${PROJECT_SOURCE_DIR}/tests")
        apply_vidchopper_display_version("${target_name}")
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
    add_vidchopper_test(vidchopper_test_encoder_model tests/test_encoder_model.cpp fast)
    add_vidchopper_test(vidchopper_test_string_utils tests/test_string_utils.cpp fast)
    add_vidchopper_test(vidchopper_test_string_utils_extended tests/test_string_utils_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_models tests/test_models.cpp fast)
    add_vidchopper_test(vidchopper_test_ready_marker tests/test_ready_marker.cpp fast)
    add_vidchopper_test(vidchopper_test_enum_clamp tests/test_enum_clamp.cpp fast)
    add_vidchopper_test(vidchopper_test_demo_launch_options tests/test_demo_launch_options.cpp fast)
    add_vidchopper_test(vidchopper_test_process_runner tests/test_process_runner.cpp fast vidchopper_services)
    add_vidchopper_test(vidchopper_test_probe_service tests/test_probe_service.cpp fast vidchopper_services)
    add_vidchopper_test(
        vidchopper_test_output_planner_golden tests/test_output_planner_golden.cpp fast vidchopper_services
    )
    add_vidchopper_test(vidchopper_test_export_runner tests/test_export_runner.cpp fast vidchopper_services)
    add_vidchopper_test(vidchopper_test_manifest_writer tests/test_manifest_writer.cpp fast vidchopper_services)
    if(TARGET vidchopper_cli)
        add_vidchopper_test(vidchopper_test_cli_arguments tests/test_cli_arguments.cpp fast vidchopper_cli)
        add_vidchopper_test(vidchopper_test_cli_settings tests/test_cli_settings.cpp fast vidchopper_cli)
        add_vidchopper_test(vidchopper_test_cli_contract tests/test_cli_contract.cpp fast vidchopper_cli)
        add_vidchopper_test(vidchopper_test_batch_resolver tests/test_batch_resolver.cpp fast vidchopper_cli)
        add_vidchopper_test(vidchopper_test_chapter_config tests/test_chapter_config.cpp fast vidchopper_cli)
        add_vidchopper_test(
            vidchopper_test_chapter_source_policy tests/test_chapter_source_policy.cpp fast vidchopper_cli
        )
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
        src/qt/services/probe_coordinator.cpp
        src/qt/services/probe_coordinator.hpp
        src/qt/services/session_controller.cpp
        src/qt/services/session_controller.hpp
        src/qt/services/gpu_detector.cpp
        src/qt/services/gpu_detector.hpp
        src/qt/ui/advanced_settings_dialog.cpp
        src/qt/ui/advanced_settings_dialog.hpp
        src/qt/ui/chapter_table_model.cpp
        src/qt/ui/chapter_table_model.hpp
        src/qt/ui/demo_automation_controller.cpp
        src/qt/ui/demo_automation_controller.hpp
        src/qt/ui/export_start_policy.hpp
        src/qt/ui/log_view_controller.cpp
        src/qt/ui/log_view_controller.hpp
        src/qt/ui/main_window.cpp
        src/qt/ui/main_window.hpp
        src/qt/ui/presentation_controller.cpp
        src/qt/ui/presentation_controller.hpp
        src/qt/ui/zoom_policy.cpp
        src/qt/ui/zoom_policy.hpp
        resources/app_icon.qrc
        ${VIDCHOPPER_WIN_RESOURCE}
    )

    target_link_libraries(VidChopper PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets vidchopper_core vidchopper_services)
    apply_vidchopper_display_version(VidChopper)
    apply_vidchopper_warnings(VidChopper)

    set_target_properties(VidChopper PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME "VidChopper")

    if(VIDCHOPPER_BUILD_QT_TESTS)
        find_package(Qt6 6.7 REQUIRED COMPONENTS Test)

        add_executable(vidchopper_test_app_settings
            tests/test_app_settings.cpp
            src/qt/app_settings.cpp
            src/qt/app_settings.hpp
            src/qt/ui/zoom_policy.cpp
            src/qt/ui/zoom_policy.hpp
        )
        target_include_directories(vidchopper_test_app_settings PRIVATE "${PROJECT_SOURCE_DIR}/tests")
        target_link_libraries(vidchopper_test_app_settings PRIVATE Qt6::Core vidchopper_core)
        apply_vidchopper_display_version(vidchopper_test_app_settings)
        apply_vidchopper_warnings(vidchopper_test_app_settings)
        add_test(NAME qt.vidchopper_test_app_settings COMMAND vidchopper_test_app_settings)
        set_tests_properties(qt.vidchopper_test_app_settings PROPERTIES LABELS "qt")

        add_executable(vidchopper_test_chapter_table_model
            tests/test_chapter_table_model.cpp
            src/qt/ui/chapter_table_model.cpp
            src/qt/ui/chapter_table_model.hpp
        )
        target_include_directories(vidchopper_test_chapter_table_model PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/tests"
        )
        target_link_libraries(vidchopper_test_chapter_table_model PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            vidchopper_core
        )
        apply_vidchopper_display_version(vidchopper_test_chapter_table_model)
        apply_vidchopper_warnings(vidchopper_test_chapter_table_model)
        add_test(NAME qt.vidchopper_test_chapter_table_model COMMAND vidchopper_test_chapter_table_model)
        set_tests_properties(qt.vidchopper_test_chapter_table_model PROPERTIES LABELS "qt")

        add_executable(vidchopper_test_log_view_controller
            tests/test_log_view_controller.cpp
            src/qt/ui/log_view_controller.cpp
            src/qt/ui/log_view_controller.hpp
        )
        target_include_directories(vidchopper_test_log_view_controller PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/tests"
        )
        target_link_libraries(vidchopper_test_log_view_controller PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
        )
        apply_vidchopper_display_version(vidchopper_test_log_view_controller)
        apply_vidchopper_warnings(vidchopper_test_log_view_controller)
        add_test(NAME qt.vidchopper_test_log_view_controller COMMAND vidchopper_test_log_view_controller)
        set_tests_properties(qt.vidchopper_test_log_view_controller PROPERTIES LABELS "qt")

        add_executable(vidchopper_test_probe_coordinator
            tests/test_probe_coordinator.cpp
            src/qt/services/probe_coordinator.cpp
            src/qt/services/probe_coordinator.hpp
        )
        target_include_directories(vidchopper_test_probe_coordinator PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/tests"
        )
        target_link_libraries(vidchopper_test_probe_coordinator PRIVATE
            Qt6::Core
            Qt6::Test
            vidchopper_services
        )
        apply_vidchopper_display_version(vidchopper_test_probe_coordinator)
        apply_vidchopper_warnings(vidchopper_test_probe_coordinator)
        add_test(NAME qt.vidchopper_test_probe_coordinator COMMAND vidchopper_test_probe_coordinator)
        set_tests_properties(qt.vidchopper_test_probe_coordinator PROPERTIES LABELS "qt")

        add_executable(vidchopper_test_export_coordinator
            tests/test_export_coordinator.cpp
            src/qt/services/export_coordinator.cpp
            src/qt/services/export_coordinator.hpp
        )
        target_include_directories(vidchopper_test_export_coordinator PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/tests"
        )
        target_link_libraries(vidchopper_test_export_coordinator PRIVATE
            Qt6::Core
            Qt6::Test
            vidchopper_services
        )
        apply_vidchopper_display_version(vidchopper_test_export_coordinator)
        apply_vidchopper_warnings(vidchopper_test_export_coordinator)
        add_test(NAME qt.vidchopper_test_export_coordinator COMMAND vidchopper_test_export_coordinator)
        set_tests_properties(qt.vidchopper_test_export_coordinator PROPERTIES LABELS "qt")
    endif()
endif()
