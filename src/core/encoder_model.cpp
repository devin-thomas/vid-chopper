#include "core/encoder_model.hpp"

#include "core/string_utils.hpp"

#include <string>

namespace vidchopper {

namespace {

constexpr auto platform_windows = u8 {1U << static_cast<u8>(EncoderPlatform::Windows)};
constexpr auto platform_linux = u8 {1U << static_cast<u8>(EncoderPlatform::Linux)};
constexpr auto platform_macos = u8 {1U << static_cast<u8>(EncoderPlatform::MacOs)};
constexpr auto platform_other = u8 {1U << static_cast<u8>(EncoderPlatform::Other)};
constexpr auto platform_all = u8 {platform_windows | platform_linux | platform_macos | platform_other};

[[nodiscard]] auto auto_descriptor() -> EncoderDescriptor {
    return EncoderDescriptor {
        .kind = EncoderKind::Auto,
        .display_name = "Auto",
        .quality_semantics = EncoderQualitySemantics::None,
        .platform_mask = platform_all,
        .enabled_in_release = true,
    };
}

[[nodiscard]] auto x264_descriptor() -> EncoderDescriptor {
    return EncoderDescriptor {
        .kind = EncoderKind::X264,
        .display_name = "x264",
        .codec_name = "libx264",
        .quality_semantics = EncoderQualitySemantics::ConstantRateFactor,
        .quality_name = "CRF",
        .quality_argument = "-crf",
        .quality_default = 18,
        .quality_minimum = 0,
        .quality_maximum = 51,
        .default_preset = "slow",
        .platform_mask = platform_all,
        .enabled_in_release = true,
    };
}

[[nodiscard]] auto nvenc_descriptor() -> EncoderDescriptor {
    return EncoderDescriptor {
        .kind = EncoderKind::HevcNvenc,
        .display_name = "HEVC NVENC",
        .codec_name = "hevc_nvenc",
        .quality_semantics = EncoderQualitySemantics::ConstantQuantizer,
        .quality_name = "CQ",
        .quality_argument = "-cq",
        .quality_default = 22,
        .quality_minimum = 0,
        .quality_maximum = 51,
        .default_preset = "p5",
        .platform_mask = platform_windows | platform_linux,
        .hardware_accelerated = true,
        .enabled_in_release = true,
    };
}

[[nodiscard]] auto videotoolbox_descriptor() -> EncoderDescriptor {
    return EncoderDescriptor {
        .kind = EncoderKind::HevcVideoToolbox,
        .display_name = "HEVC VideoToolbox",
        .codec_name = "hevc_videotoolbox",
        .quality_semantics = EncoderQualitySemantics::VideoToolboxQuality,
        .quality_name = "Quality",
        .quality_argument = "-q:v",
        .quality_default = 65,
        .quality_minimum = 1,
        .quality_maximum = 100,
        .platform_mask = platform_macos,
        .hardware_accelerated = true,
        .enabled_in_release = false,
    };
}

[[nodiscard]] auto normalized_encoder_name(const std::string_view name) -> std::string {
    auto normalized = to_lower_copy(trim_copy(name));
    for (char& character : normalized) {
        if (character == '-' || character == ' ') {
            character = '_';
        }
    }
    return normalized;
}

} // namespace

auto encoder_descriptor(const EncoderKind kind) -> EncoderDescriptor {
    switch (kind) {
    case EncoderKind::Auto:
        return auto_descriptor();
    case EncoderKind::X264:
        return x264_descriptor();
    case EncoderKind::HevcNvenc:
        return nvenc_descriptor();
    case EncoderKind::HevcVideoToolbox:
        return videotoolbox_descriptor();
    }
    return auto_descriptor();
}

auto encoder_kind_name(const EncoderKind kind) -> std::string_view {
    switch (kind) {
    case EncoderKind::Auto:
        return "auto";
    case EncoderKind::X264:
        return "x264";
    case EncoderKind::HevcNvenc:
        return "hevc_nvenc";
    case EncoderKind::HevcVideoToolbox:
        return "hevc_videotoolbox";
    }
    return "unknown";
}

auto encoder_platform_name(const EncoderPlatform platform) -> std::string_view {
    switch (platform) {
    case EncoderPlatform::Unknown:
        return "unknown";
    case EncoderPlatform::Windows:
        return "windows";
    case EncoderPlatform::Linux:
        return "linux";
    case EncoderPlatform::MacOs:
        return "macos";
    case EncoderPlatform::Other:
        return "other";
    }
    return "unknown";
}

auto current_encoder_platform() noexcept -> EncoderPlatform {
#ifdef _WIN32
    return EncoderPlatform::Windows;
#elif defined(__APPLE__)
    return EncoderPlatform::MacOs;
#elif defined(__linux__)
    return EncoderPlatform::Linux;
#else
    return EncoderPlatform::Other;
#endif
}

auto encoder_platform_eligible(const EncoderDescriptor& descriptor, const EncoderPlatform platform) noexcept -> bool {
    if (platform == EncoderPlatform::Unknown) {
        return true;
    }
    const auto bit = static_cast<u8>(1U << static_cast<u8>(platform));
    return (descriptor.platform_mask & bit) != 0;
}

auto encoder_kind_is_known(const EncoderKind kind) noexcept -> bool {
    switch (kind) {
    case EncoderKind::Auto:
    case EncoderKind::X264:
    case EncoderKind::HevcNvenc:
    case EncoderKind::HevcVideoToolbox:
        return true;
    }
    return false;
}

auto encoder_kind_from_name(const std::string_view name) -> std::optional<EncoderKind> {
    const std::string normalized = normalized_encoder_name(name);
    if (normalized == "auto") {
        return EncoderKind::Auto;
    }
    if (normalized == "x264" || normalized == "libx264") {
        return EncoderKind::X264;
    }
    if (normalized == "hevc_nvenc" || normalized == "nvenc") {
        return EncoderKind::HevcNvenc;
    }
    if (normalized == "hevc_videotoolbox" || normalized == "video_toolbox" || normalized == "videotoolbox") {
        return EncoderKind::HevcVideoToolbox;
    }
    return std::nullopt;
}

auto encoder_kind_from_persisted_value(
    const i64 raw_value, const EncoderKind fallback, std::string& diagnostic) -> EncoderKind {
    if (raw_value >= static_cast<i64>(EncoderKind::Auto) && raw_value <= static_cast<i64>(EncoderKind::HevcNvenc)) {
        return static_cast<EncoderKind>(raw_value);
    }

    diagnostic = "Unknown persisted encoder value " + std::to_string(raw_value) + "; using "
        + std::string {encoder_kind_name(fallback)}
        + ". Supported 1.1 values are 0 (auto), 1 (x264), and 2 (hevc_nvenc).";
    return fallback;
}

auto encoder_arguments_for(const ExportSettings& settings, const EncoderKind kind) -> std::vector<std::string> {
    const EncoderDescriptor descriptor = encoder_descriptor(kind);
    if (!descriptor.enabled_in_release || kind == EncoderKind::Auto) {
        return {};
    }

    const std::string preset = std::string {encoder_preset_value(settings, kind)};
    const std::string quality = std::to_string(encoder_quality_value(settings, kind));
    if (kind == EncoderKind::HevcVideoToolbox) {
        return {std::string {descriptor.quality_argument}, quality};
    }

    auto arguments = std::vector<std::string> {"-preset", preset, std::string {descriptor.quality_argument}, quality};
    if (kind == EncoderKind::HevcNvenc) {
        arguments.insert(arguments.end(), {"-rc", "vbr_hq"});
    }
    return arguments;
}

auto encoder_quality_value(const ExportSettings& settings, const EncoderKind kind) noexcept -> u8 {
    switch (kind) {
    case EncoderKind::X264:
        return settings.x264_crf;
    case EncoderKind::HevcNvenc:
        return settings.nvenc_cq;
    case EncoderKind::HevcVideoToolbox:
        return settings.video_toolbox_quality;
    case EncoderKind::Auto:
        return 0;
    }
    return 0;
}

auto encoder_preset_value(const ExportSettings& settings, const EncoderKind kind) -> std::string_view {
    switch (kind) {
    case EncoderKind::X264:
        return settings.x264_preset;
    case EncoderKind::HevcNvenc:
        return settings.nvenc_preset;
    case EncoderKind::HevcVideoToolbox:
    case EncoderKind::Auto:
        return settings.video_toolbox_preset;
    }
    return {};
}

} // namespace vidchopper
