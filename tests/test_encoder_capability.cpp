#include "core/path_utils.hpp"
#include "services/encoder_capability.hpp"
#include "test_support.hpp"

#include <string>
#include <string_view>
#include <vector>

using namespace vidchopper;

namespace {

struct CapabilityFixture {
    std::vector<ProcessRequest> requests;
    bool fail_nvenc {false};
    bool list_nvenc {true};
};

[[nodiscard]] auto contains_argument(const std::vector<std::string>& arguments, const std::string_view value) -> bool {
    for (const std::string& argument : arguments) {
        if (argument == value) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto fixture_executor(CapabilityFixture& fixture) -> ProcessExecutor {
    return [&fixture](const ProcessRequest& request) -> ProcessResult {
        fixture.requests.push_back(request);
        if (contains_argument(request.arguments, "-encoders")) {
            return ProcessResult {
                .state = ProcessExitState::Success,
                .standard_output = fixture.list_nvenc ? "libx264 hevc_nvenc" : "libx264",
            };
        }
        if (fixture.fail_nvenc && contains_argument(request.arguments, "hevc_nvenc")) {
            return ProcessResult {
                .state = ProcessExitState::NonzeroExit,
                .exit_code = 1,
                .standard_error = "encoder unavailable on this fixture host",
            };
        }
        return ProcessResult {.state = ProcessExitState::Success};
    };
}

} // namespace

auto main() -> int {
    auto settings = ExportSettings {};
    settings.ffmpeg_path = path_to_utf8(path_from_utf8("portable tools/ffmpeg 🎬"));
    settings.x264_preset = "fast";
    settings.x264_crf = 31;

    CapabilityFixture x264_fixture {};
    const EncoderCapabilityResult x264 = EncoderCapabilityService {fixture_executor(x264_fixture)}.test(settings,
        EncoderKind::X264,
        EncoderEnvironment {.platform = EncoderPlatform::Linux},
        EncoderCapabilityOptions {.use_encoder_listing_prefilter = true});
    test_support::expect_true(x264.available(), "x264 capability should pass a successful minimal encode");
    test_support::expect_true(x264.listing_checked, "the optional encoder listing prefilter should be recorded");
    test_support::expect_true(x264.listed_by_ffmpeg, "the x264 fixture should be listed");
    test_support::expect_eq(
        x264_fixture.requests.size(), size_t {2}, "a listed backend should still receive the real capability encode");
    test_support::expect_eq(x264.command.front(),
        settings.ffmpeg_path,
        "capability command should preserve a spaced Unicode executable path");
    test_support::expect_true(contains_argument(x264.command, "libx264"), "x264 capability should use the x264 codec");
    test_support::expect_true(contains_argument(x264.command, "-crf"), "x264 capability should use CRF settings");
    test_support::expect_true(
        !contains_argument(x264.command, "-cq"), "x264 capability should not use NVENC CQ settings");

    CapabilityFixture missing_listing_fixture {.list_nvenc = false};
    const EncoderCapabilityResult missing_listing =
        EncoderCapabilityService {fixture_executor(missing_listing_fixture)}.test(settings,
            EncoderKind::HevcNvenc,
            EncoderEnvironment {.platform = EncoderPlatform::Linux},
            EncoderCapabilityOptions {.use_encoder_listing_prefilter = true});
    test_support::expect_true(!missing_listing.ok(), "an unlisted hardware backend should be unavailable");
    test_support::expect_eq(missing_listing.status,
        EncoderCapabilityStatus::Unavailable,
        "a listing prefilter rejection should be unavailable, not a successful capability");
    test_support::expect_eq(
        missing_listing_fixture.requests.size(), size_t {1}, "an unlisted backend should not start the minimal encode");
    test_support::expect_true(missing_listing.rejection_reason.find("hevc_nvenc") != std::string::npos,
        "listing rejection should identify the missing codec");

    CapabilityFixture fallback_fixture {.fail_nvenc = true};
    settings.encoder_kind = EncoderKind::Auto;
    const EncoderSelectionResult fallback =
        EncoderCapabilityService {fixture_executor(fallback_fixture)}.select(settings,
            EncoderEnvironment {
                .has_nvidia_gpu = true,
                .has_hevc_nvenc_encoder = true,
                .platform = EncoderPlatform::Linux,
            });
    test_support::expect_true(fallback.ok(), "Auto should fall back to x264 after hardware capability failure");
    test_support::expect_eq(
        fallback.selection.requested_kind, EncoderKind::Auto, "fallback should retain the requested Auto policy");
    test_support::expect_eq(fallback.selection.resolved_kind, EncoderKind::X264, "fallback should resolve to x264");
    test_support::expect_true(fallback.selection.used_fallback, "fallback should be explicit in the selection");
    test_support::expect_eq(fallback.capability_results.size(),
        size_t {2},
        "Auto fallback should record both hardware and x264 capability attempts");
    test_support::expect_true(
        fallback.summary.find("x264") != std::string::npos, "fallback summary should name the resolved backend");

    settings.encoder_kind = EncoderKind::HevcNvenc;
    CapabilityFixture explicit_failure_fixture {.fail_nvenc = true};
    const EncoderSelectionResult explicit_failure =
        EncoderCapabilityService {fixture_executor(explicit_failure_fixture)}.select(
            settings, EncoderEnvironment {.platform = EncoderPlatform::Linux});
    test_support::expect_true(!explicit_failure.ok(), "an explicit failed hardware backend should block export");
    test_support::expect_eq(
        explicit_failure.capability_results.size(), size_t {1}, "explicit failure should not silently try x264");
    test_support::expect_true(explicit_failure.error_message.find("stored encoder preference") != std::string::npos,
        "explicit failure should explain that the stored preference remains unchanged");
    test_support::expect_eq(settings.encoder_kind,
        EncoderKind::HevcNvenc,
        "capability failure should not mutate the persisted encoder preference");

    CapabilityFixture mac_fixture {};
    const EncoderCapabilityResult mac_nvenc = EncoderCapabilityService {fixture_executor(mac_fixture)}.test(
        settings, EncoderKind::HevcNvenc, EncoderEnvironment {.platform = EncoderPlatform::MacOs});
    test_support::expect_eq(
        mac_nvenc.status, EncoderCapabilityStatus::Unsupported, "NVENC should be policy-ineligible on macOS");
    test_support::expect_true(
        mac_fixture.requests.empty(), "an ineligible backend should not launch a capability process");

    const EncoderCapabilityResult disabled_videotoolbox = EncoderCapabilityService {fixture_executor(mac_fixture)}.test(
        settings, EncoderKind::HevcVideoToolbox, EncoderEnvironment {.platform = EncoderPlatform::MacOs});
    test_support::expect_eq(disabled_videotoolbox.status,
        EncoderCapabilityStatus::Unsupported,
        "disabled VideoToolbox should remain policy-ineligible in the 1.1 release");

    return 0;
}
