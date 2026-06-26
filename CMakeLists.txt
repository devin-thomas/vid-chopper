cmake_minimum_required(VERSION 3.28)

project(
    VidChopper
    VERSION 0.1.0
    DESCRIPTION "Qt desktop app for splitting videos into chapter clips with ffmpeg"
    LANGUAGES CXX
)

option(VIDCHOPPER_BUILD_GUI "Build the Qt GUI target" ON)
option(VIDCHOPPER_BUILD_TESTS "Build the test targets" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vidchopper_core
    src/core/chapter_plan.cpp
    src/core/chapter_plan.h
    src/core/command_builder.cpp
    src/core/command_builder.h
    src/core/models.h
    src/core/string_utils.cpp
    src/core/string_utils.h
    src/core/timecode.cpp
    src/core/timecode.h
    src/core/types.h
)

target_include_directories(vidchopper_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(vidchopper_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(vidchopper_core PRIVATE /W4 /permissive- /EHsc /MP /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(vidchopper_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(VIDCHOPPER_BUILD_TESTS)
    enable_testing()

    function(add_vidchopper_test target_name source_file label_name)
        add_executable("${target_name}" "${source_file}")
        target_link_libraries("${target_name}" PRIVATE vidchopper_core)
        target_include_directories("${target_name}" PRIVATE "${PROJECT_SOURCE_DIR}/tests")

        if(MSVC)
            target_compile_options("${target_name}" PRIVATE /W4 /permissive- /EHsc /MP /utf-8 /Zc:__cplusplus)
        else()
            target_compile_options("${target_name}" PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME "${label_name}.${target_name}" COMMAND "${target_name}")
        set_tests_properties("${label_name}.${target_name}" PROPERTIES LABELS "${label_name}")
    endfunction()

    add_vidchopper_test(vidchopper_test_timecode tests/test_timecode.cpp fast)
    add_vidchopper_test(vidchopper_test_timecode_extended tests/test_timecode_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_chapter_plan tests/test_chapter_plan.cpp fast)
    add_vidchopper_test(vidchopper_test_chapter_plan_extended tests/test_chapter_plan_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_command_builder tests/test_command_builder.cpp fast)
    add_vidchopper_test(vidchopper_test_command_builder_extended tests/test_command_builder_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_command_builder_golden tests/test_command_builder_golden.cpp fast)
    add_vidchopper_test(vidchopper_test_string_utils tests/test_string_utils.cpp fast)
    add_vidchopper_test(vidchopper_test_string_utils_extended tests/test_string_utils_extended.cpp fast)
    add_vidchopper_test(vidchopper_test_models tests/test_models.cpp fast)
    add_vidchopper_test(vidchopper_test_ffmpeg_integration tests/test_ffmpeg_integration.cpp slow)
endif()

if(VIDCHOPPER_BUILD_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets)

    qt_standard_project_setup()

    add_executable(VidChopper
        src/qt/main.cpp
        src/qt/app_settings.cpp
        src/qt/app_settings.h
        src/qt/dark_palette.cpp
        src/qt/dark_palette.h
        src/qt/services/export_coordinator.cpp
        src/qt/services/export_coordinator.h
        src/qt/services/ffprobe_service.cpp
        src/qt/services/ffprobe_service.h
        src/qt/services/gpu_detector.cpp
        src/qt/services/gpu_detector.h
        src/qt/ui/advanced_settings_dialog.cpp
        src/qt/ui/advanced_settings_dialog.h
        src/qt/ui/chapter_table_model.cpp
        src/qt/ui/chapter_table_model.h
        src/qt/ui/main_window.cpp
        src/qt/ui/main_window.h
    )

    target_link_libraries(VidChopper PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets vidchopper_core)

    if(MSVC)
        target_compile_options(VidChopper PRIVATE /W4 /permissive- /EHsc /MP /utf-8 /Zc:__cplusplus)
    else()
        target_compile_options(VidChopper PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    set_target_properties(VidChopper PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME "VidChopper")
endif()
