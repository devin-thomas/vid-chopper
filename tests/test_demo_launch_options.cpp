#include "qt/demo_launch_options.hpp"
#include "test_support.hpp"

#include <array>
#include <filesystem>
#include <string>

using namespace vidchopper;

namespace {

[[nodiscard]] auto fixture_path(const std::string_view filename) -> std::string {
#ifdef _WIN32
    return std::string {"C:\\capture\\"} + std::string {filename};
#else
    return std::string {"/tmp/capture/"} + std::string {filename};
#endif
}

auto expect_parse_success(const DemoLaunchOptionsParseResult& result) -> void {
    test_support::expect_true(result.success, "demo option parsing should succeed");
}

auto expect_parse_failure(const DemoLaunchOptionsParseResult& result) -> void {
    test_support::expect_true(!result.success, "demo option parsing should fail");
}

} // namespace

auto main() -> int {
    const Path demo_source =
        std::filesystem::temp_directory_path() / path_from_utf8("vidchopper demo fixtures/媒体 source 🎬.mp4");
    const Path demo_ready_file =
        std::filesystem::temp_directory_path() / path_from_utf8("vidchopper demo fixtures/ready marker.txt");
    const std::string demo_source_argument = "--demo-source=" + path_to_utf8(demo_source);
    const std::string demo_ready_file_argument = "--demo-ready-file=" + path_to_utf8(demo_ready_file);

    {
        auto first = std::string {"vidchopper"};
        auto second = std::string {"--demo-scene=workspace-logs"};
        auto third = demo_source_argument;
        auto fourth = std::string {"--window-size=1280x900"};
        auto fifth = demo_ready_file_argument;
        auto argv = std::array<char*, 5> {
            first.data(),
            second.data(),
            third.data(),
            fourth.data(),
            fifth.data(),
        };

        const auto result = parse_demo_launch_options(static_cast<int>(argv.size()), argv.data());
        expect_parse_success(result);
        test_support::expect_eq(result.options.scene, DemoScene::WorkspaceLogs, "workspace logs scene should parse");
        test_support::expect_true(
            result.options.window_size.has_value(), "window size should be available for valid demo arguments");
        test_support::expect_eq(result.options.window_size->width, 1280, "window width should parse");
        test_support::expect_eq(result.options.window_size->height, 900, "window height should parse");
    }

    {
        auto first = std::string {"vidchopper"};
        auto second = std::string {"--demo-scene=settings-precision"};
        auto third = std::string {"--demo-source=sample.mp4"};
        auto fourth = demo_ready_file_argument;
        auto argv = std::array<char*, 4> {
            first.data(),
            second.data(),
            third.data(),
            fourth.data(),
        };

        const auto result = parse_demo_launch_options(static_cast<int>(argv.size()), argv.data());
        expect_parse_failure(result);
    }

    {
        auto first = std::string {"vidchopper"};
        auto second = std::string {"--window-size=wide"};
        auto third = std::string {"--demo-scene=workspace"};
        auto fourth = demo_source_argument;
        auto fifth = demo_ready_file_argument;
        auto argv = std::array<char*, 5> {
            first.data(),
            second.data(),
            third.data(),
            fourth.data(),
            fifth.data(),
        };

        const auto result = parse_demo_launch_options(static_cast<int>(argv.size()), argv.data());
        expect_parse_failure(result);
    }

    {
        auto first = std::string {"vidchopper"};
        auto second = std::string {"--demo-scene=workspace"};
        auto third = demo_source_argument;
        auto fourth = std::string {"--window-size=1440X960"};
        auto fifth = demo_ready_file_argument;
        auto argv = std::array<char*, 5> {
            first.data(),
            second.data(),
            third.data(),
            fourth.data(),
            fifth.data(),
        };

        const auto result = parse_demo_launch_options(static_cast<int>(argv.size()), argv.data());
        expect_parse_success(result);
        test_support::expect_true(result.options.window_size.has_value(), "uppercase X should parse");
        test_support::expect_eq(result.options.window_size->width, 1440, "uppercase width should parse");
        test_support::expect_eq(result.options.window_size->height, 960, "uppercase height should parse");
    }

    {
        auto first = std::string {"vidchopper"};
        auto second = std::string {"--style=fusion"};
        auto argv = std::array<char*, 2> {
            first.data(),
            second.data(),
        };

        const auto result = parse_demo_launch_options(static_cast<int>(argv.size()), argv.data());
        expect_parse_success(result);
        test_support::expect_eq(result.options.scene, DemoScene::None, "non-demo arguments should be ignored");
    }

    return 0;
}
