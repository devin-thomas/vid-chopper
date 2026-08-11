#pragma once

#include "core/models.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vidchopper {

struct EncoderDescriptor {
    EncoderKind kind {EncoderKind::Auto};
    std::string_view display_name;
    std::string_view codec_name;
    EncoderQualitySemantics quality_semantics {EncoderQualitySemantics::None};
    std::string_view quality_name;
    std::string_view quality_argument;
    u8 quality_default {0};
    u8 quality_minimum {0};
    u8 quality_maximum {0};
    std::string_view default_preset;
    u8 platform_mask {0};
    bool hardware_accelerated {false};
    bool enabled_in_release {false};
};

[[nodiscard]] auto encoder_descriptor(EncoderKind kind) -> EncoderDescriptor;
[[nodiscard]] auto encoder_kind_name(EncoderKind kind) -> std::string_view;
[[nodiscard]] auto encoder_platform_name(EncoderPlatform platform) -> std::string_view;
[[nodiscard]] auto current_encoder_platform() noexcept -> EncoderPlatform;
[[nodiscard]] auto encoder_platform_eligible(const EncoderDescriptor& descriptor, EncoderPlatform platform) noexcept
    -> bool;
[[nodiscard]] auto encoder_kind_is_known(EncoderKind kind) noexcept -> bool;
[[nodiscard]] auto encoder_kind_from_name(std::string_view name) -> std::optional<EncoderKind>;
[[nodiscard]] auto encoder_kind_from_persisted_value(
    i64 raw_value, EncoderKind fallback, std::string& diagnostic) -> EncoderKind;
[[nodiscard]] auto encoder_arguments_for(const ExportSettings& settings, EncoderKind kind) -> std::vector<std::string>;
[[nodiscard]] auto encoder_quality_value(const ExportSettings& settings, EncoderKind kind) noexcept -> u8;
[[nodiscard]] auto encoder_preset_value(const ExportSettings& settings, EncoderKind kind) -> std::string_view;

} // namespace vidchopper
