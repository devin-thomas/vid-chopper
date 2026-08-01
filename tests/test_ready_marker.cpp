#include "core/ready_marker.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace vidchopper;

auto main() -> int {
    const Path root = std::filesystem::temp_directory_path() / "vidchopper-ready-marker";
    auto cleanup_error = std::error_code {};
    std::filesystem::remove_all(root, cleanup_error);

    const Path marker = root / "nested" / "ready.txt";
    const auto written = write_ready_marker(marker, "ready");
    test_support::expect_true(written.ok(), "ready marker should be written atomically");
    auto input = std::ifstream {marker, std::ios::binary};
    const auto content = std::string {std::istreambuf_iterator<char> {input}, std::istreambuf_iterator<char> {}};
    test_support::expect_eq(content, std::string {"ready\n"}, "ready marker should contain the complete status");
    test_support::expect_true(!std::filesystem::exists(marker.string() + ".tmp"),
        "successful ready-marker write should not leave a temporary file");

    const Path blocking_file = root / "blocking-file";
    std::ofstream {blocking_file} << "not a directory";
    const auto blocked = write_ready_marker(blocking_file / "ready.txt", "ready");
    test_support::expect_true(!blocked.ok(), "unwritable ready-marker parent should fail");
    test_support::expect_true(blocked.error_message.find(blocking_file.string()) != std::string::npos,
        "ready-marker failure should identify the blocked path");

    std::filesystem::remove_all(root, cleanup_error);
    return 0;
}
