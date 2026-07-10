#include "qt/demo_launch_options.h"
#include "test_support.h"

#include <array>

using namespace vidchopper;

namespace {

auto expect_parse_success(const DemoLaunchOptionsParseResult& result) -> void {
    test_support::expect_true(result.success, "demo option parsing should succeed");
}

auto expect_parse_failure(const DemoLaunchOptionsParseResult& result) -> void {
    test_support::expect_true(!result.success, "demo option parsing should fail");
}

} // namespace

auto main() -> int {
    {
        auto first = std::string {"vidchopper.exe"};
        auto second = std::string {"--demo-scene=workspace-logs"};
        auto third = std::string {"--demo-source=C:\\capture\\sample.mp4"};
        auto fourth = std::string {"--window-size=1280x900"};
        auto fifth = std::string {"--demo-ready-file=C:\\capture\\ready.txt"};
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
        auto first = std::string {"vidchopper.exe"};
        auto second = std::string {"--demo-scene=settings-precision"};
        auto third = std::string {"--demo-source=sample.mp4"};
        auto fourth = std::string {"--demo-ready-file=C:\\capture\\ready.txt"};
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
        auto first = std::string {"vidchopper.exe"};
        auto second = std::string {"--window-size=wide"};
        auto third = std::string {"--demo-scene=workspace"};
        auto fourth = std::string {"--demo-source=C:\\capture\\sample.mp4"};
        auto fifth = std::string {"--demo-ready-file=C:\\capture\\ready.txt"};
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
        auto first = std::string {"vidchopper.exe"};
        auto second = std::string {"--demo-scene=workspace"};
        auto third = std::string {"--demo-source=C:\\capture\\sample.mp4"};
        auto fourth = std::string {"--window-size=1440X960"};
        auto fifth = std::string {"--demo-ready-file=C:\\capture\\ready.txt"};
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
        auto first = std::string {"vidchopper.exe"};
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
