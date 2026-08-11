#include "core/command_builder.hpp"
#include "core/encoder_model.hpp"
#include "test_support.hpp"

#include <string>
#include <vector>

using namespace vidchopper;

namespace {

auto expect_no_diagnostic(const std::string& diagnostic, const std::string_view message) -> void {
    test_support::expect_true(diagnostic.empty(), message);
}

} // namespace

static_assert(static_cast<int>(EncoderKind::Auto) == 0);
static_assert(static_cast<int>(EncoderKind::X264) == 1);
static_assert(static_cast<int>(EncoderKind::HevcNvenc) == 2);
static_assert(static_cast<int>(EncoderKind::HevcVideoToolbox) == 3);

auto main() -> int {
    const EncoderDescriptor x264 = encoder_descriptor(EncoderKind::X264);
    test_support::expect_eq(x264.display_name, std::string_view {"x264"}, "x264 display name");
    test_support::expect_eq(x264.codec_name, std::string_view {"libx264"}, "x264 codec name");
    test_support::expect_eq(
        x264.quality_semantics, EncoderQualitySemantics::ConstantRateFactor, "x264 must own CRF quality semantics");
    test_support::expect_eq(x264.quality_argument, std::string_view {"-crf"}, "x264 quality argument");
    test_support::expect_eq(x264.quality_default, u8 {18}, "x264 default CRF");
    test_support::expect_eq(x264.quality_minimum, u8 {0}, "x264 minimum CRF");
    test_support::expect_eq(x264.quality_maximum, u8 {51}, "x264 maximum CRF");

    const EncoderDescriptor nvenc = encoder_descriptor(EncoderKind::HevcNvenc);
    test_support::expect_eq(nvenc.display_name, std::string_view {"HEVC NVENC"}, "NVENC display name");
    test_support::expect_eq(nvenc.codec_name, std::string_view {"hevc_nvenc"}, "NVENC codec name");
    test_support::expect_eq(
        nvenc.quality_semantics, EncoderQualitySemantics::ConstantQuantizer, "NVENC must own CQ quality semantics");
    test_support::expect_eq(nvenc.quality_argument, std::string_view {"-cq"}, "NVENC quality argument");
    test_support::expect_true(
        encoder_platform_eligible(nvenc, EncoderPlatform::Windows), "NVENC should be eligible on Windows");
    test_support::expect_true(
        encoder_platform_eligible(nvenc, EncoderPlatform::Linux), "NVENC should be eligible on Linux");
    test_support::expect_true(
        !encoder_platform_eligible(nvenc, EncoderPlatform::MacOs), "NVENC should not be eligible on macOS");

    const EncoderDescriptor videotoolbox = encoder_descriptor(EncoderKind::HevcVideoToolbox);
    test_support::expect_eq(
        videotoolbox.display_name, std::string_view {"HEVC VideoToolbox"}, "VideoToolbox display name");
    test_support::expect_eq(videotoolbox.codec_name, std::string_view {"hevc_videotoolbox"}, "VideoToolbox codec name");
    test_support::expect_eq(videotoolbox.quality_semantics,
        EncoderQualitySemantics::VideoToolboxQuality,
        "VideoToolbox must own a distinct quality semantic");
    test_support::expect_eq(videotoolbox.quality_argument, std::string_view {"-q:v"}, "VideoToolbox quality argument");
    test_support::expect_eq(videotoolbox.quality_default, u8 {65}, "VideoToolbox default quality");
    test_support::expect_eq(videotoolbox.quality_minimum, u8 {1}, "VideoToolbox minimum quality");
    test_support::expect_eq(videotoolbox.quality_maximum, u8 {100}, "VideoToolbox maximum quality");
    test_support::expect_true(
        !videotoolbox.enabled_in_release, "VideoToolbox must remain disabled before the 1.2.0 backend release");

    auto diagnostic = std::string {};
    test_support::expect_eq(encoder_kind_from_persisted_value(0, EncoderKind::Auto, diagnostic),
        EncoderKind::Auto,
        "persisted Auto value must retain its meaning");
    expect_no_diagnostic(diagnostic, "valid Auto value should not diagnose");
    test_support::expect_eq(encoder_kind_from_persisted_value(1, EncoderKind::Auto, diagnostic),
        EncoderKind::X264,
        "persisted x264 value must retain its meaning");
    expect_no_diagnostic(diagnostic, "valid x264 value should not diagnose");
    test_support::expect_eq(encoder_kind_from_persisted_value(2, EncoderKind::Auto, diagnostic),
        EncoderKind::HevcNvenc,
        "persisted NVENC value must retain its meaning");
    expect_no_diagnostic(diagnostic, "valid NVENC value should not diagnose");
    test_support::expect_eq(encoder_kind_from_persisted_value(3, EncoderKind::Auto, diagnostic),
        EncoderKind::Auto,
        "future VideoToolbox value must not be enabled by the 1.1 loader");
    test_support::expect_true(diagnostic.find("Unknown persisted encoder value 3") != std::string::npos,
        "future encoder values should produce a migration diagnostic");
    test_support::expect_eq(encoder_kind_from_persisted_value(-1, EncoderKind::Auto, diagnostic),
        EncoderKind::Auto,
        "negative persisted values must use the safe fallback");
    test_support::expect_true(diagnostic.find("Unknown persisted encoder value -1") != std::string::npos,
        "negative encoder values should produce a migration diagnostic");

    test_support::expect_eq(encoder_kind_from_name("HEVC NVENC"),
        std::optional<EncoderKind> {EncoderKind::HevcNvenc},
        "display names should map to the stored NVENC backend");
    test_support::expect_eq(encoder_kind_from_name("video-toolbox"),
        std::optional<EncoderKind> {EncoderKind::HevcVideoToolbox},
        "future VideoToolbox names should be representable");
    test_support::expect_true(!encoder_kind_from_name("not-a-backend").has_value(),
        "unknown backend names should not select an arbitrary encoder");

    auto settings = ExportSettings {};
    settings.x264_crf = 28;
    settings.nvenc_cq = 37;
    settings.video_toolbox_quality = 82;
    settings.x264_preset = "ultrafast";
    settings.nvenc_preset = "p7";

    test_support::expect_eq(encoder_arguments_for(settings, EncoderKind::X264),
        std::vector<std::string> {"-preset", "ultrafast", "-crf", "28"},
        "x264 arguments must use only x264 settings");
    test_support::expect_eq(encoder_arguments_for(settings, EncoderKind::HevcNvenc),
        std::vector<std::string> {"-preset", "p7", "-cq", "37", "-rc", "vbr_hq"},
        "NVENC arguments must preserve CQ semantics and ordering");
    test_support::expect_true(encoder_arguments_for(settings, EncoderKind::HevcVideoToolbox).empty(),
        "disabled VideoToolbox must not generate an enabled release command");

    const ResolvedEncoder resolved_nvenc =
        resolve_encoder(settings, EncoderEnvironment {.has_nvidia_gpu = true, .has_hevc_nvenc_encoder = true});
    test_support::expect_eq(resolved_nvenc.kind, EncoderKind::HevcNvenc, "Auto should preserve legacy NVENC selection");
    test_support::expect_eq(resolved_nvenc.quality_name, std::string {"CQ"}, "resolved NVENC quality label");
    test_support::expect_eq(resolved_nvenc.quality_value, u8 {37}, "resolved NVENC quality value");
    test_support::expect_eq(resolved_nvenc.preset, std::string {"p7"}, "resolved NVENC preset");

    const ResolvedEncoder resolved_videotoolbox =
        resolve_encoder(ExportSettings {.encoder_kind = EncoderKind::HevcVideoToolbox}, EncoderEnvironment {});
    test_support::expect_eq(resolved_videotoolbox.kind,
        EncoderKind::X264,
        "disabled VideoToolbox must resolve to a visible safe fallback for command construction");
    test_support::expect_true(
        resolved_videotoolbox.used_fallback, "disabled VideoToolbox command construction should mark its fallback");

    return 0;
}
