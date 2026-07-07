#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace test_support {

class DummyCliData final {
public:
    static constexpr auto input_video_name = std::string_view {"mock_stream.mkv"};
    static constexpr auto json_config_name = std::string_view {"mock_stream.json"};
    static constexpr auto yaml_config_name = std::string_view {"mock_stream.yaml"};
    static constexpr auto cli_settings_name = std::string_view {"VidChopperCLI.ini"};
    static constexpr auto gui_settings_name = std::string_view {"VidChopper.ini"};

    [[nodiscard]] static auto root() -> std::filesystem::path {
        return std::filesystem::path {"tests"} / "dummy" / "mock_data";
    }

    [[nodiscard]] static auto input_video_path() -> std::filesystem::path {
        return root() / std::string {input_video_name};
    }

    [[nodiscard]] static auto json_config_path() -> std::filesystem::path {
        return root() / std::string {json_config_name};
    }

    [[nodiscard]] static auto yaml_config_path() -> std::filesystem::path {
        return root() / std::string {yaml_config_name};
    }

    [[nodiscard]] static auto executable_path() -> std::filesystem::path {
        return root() / "bin" / "VidChopperCLI.exe";
    }

    [[nodiscard]] static auto direct_tokens() -> std::vector<std::string> {
        return {
            input_video_path().string(),
            json_config_path().string(),
        };
    }

    [[nodiscard]] static auto chop_tokens() -> std::vector<std::string> {
        return {
            "chop",
            input_video_path().string(),
            yaml_config_path().string(),
            "--dry-run",
        };
    }

    [[nodiscard]] static auto advanced_tokens() -> std::vector<std::string> {
        return {
            input_video_path().string(),
            json_config_path().string(),
            "--crf",
            "18",
            "--cq",
            "22",
            "--preset",
            "slow",
            "--threads",
            "4",
            "--use-gui-config",
            "--stop-on-first-error",
        };
    }
};

} // namespace test_support
