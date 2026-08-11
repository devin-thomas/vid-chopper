#include "services/probe_service.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

using namespace vidchopper;

namespace {

[[nodiscard]] auto fixture_text() -> std::string {
    const Path fixture = Path {__FILE__}.parent_path() / "dummy" / "mock_data" / "mock_ffprobe_embedded_chapters.json";
    auto stream = std::ifstream {fixture};
    auto output = std::ostringstream {};
    output << stream.rdbuf();
    return output.str();
}

[[nodiscard]] auto failed(ProcessExitState state, std::string error) -> ProcessResult {
    return ProcessResult {.state = state, .standard_error = std::move(error)};
}

[[nodiscard]] auto contains(const std::string_view text, const std::string_view needle) -> bool {
    return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] auto oversized_stream_output() -> std::string {
    auto output = std::string {R"({"format":{"duration":"1"},"streams":[)"};
    for (auto index = size_t {0}; index <= maximum_probe_streams; ++index) {
        if (index != 0) {
            output += ',';
        }
        output += index == 0 ? R"({"index":0,"codec_type":"video","codec_name":"h264","avg_frame_rate":"30/1"})"
                             : R"({"index":1,"codec_type":"audio","codec_name":"aac"})";
    }
    output += "]}";
    return output;
}

} // namespace

auto main() -> int {
    const auto executable = Path {R"(C:\Program Files\ffmpeg\ffprobe.exe)"};
    const auto source = Path {R"(C:\Users\RUNNER~1\AppData\Local\Temp\match clips\set.mkv)"};
    const auto success_process = ProcessResult {
        .state = ProcessExitState::Success,
        .standard_output = fixture_text(),
    };
    const ProbeResult valid = parse_probe_output(executable, source, success_process);
    test_support::expect_true(valid.ok(), "valid ffprobe fixture should parse");
    test_support::expect_eq(valid.metadata.duration_ms, u64 {180000}, "duration should parse in milliseconds");
    test_support::expect_eq(valid.metadata.frame_rate,
        FrameRate {.numerator = 30000, .denominator = 1001},
        "average frame rate should parse as a rational");
    test_support::expect_eq(valid.metadata.source_path, source, "source path identity should be preserved");
    test_support::expect_eq(valid.metadata.source_extension, std::string {".mkv"}, "extension should be normalized");
    test_support::expect_eq(valid.metadata.streams.size(), size_t {2}, "ordered stream metadata should parse");
    test_support::expect_eq(valid.metadata.streams[0],
        StreamMetadata {.index = 0, .codec_type = "video", .codec_name = "h264"},
        "video stream identity should be preserved");
    test_support::expect_eq(valid.metadata.streams[1],
        StreamMetadata {.index = 1, .codec_type = "audio", .codec_name = "aac"},
        "audio stream identity should be preserved");
    test_support::expect_eq(valid.metadata.embedded_chapters.size(), size_t {3}, "ordered chapters should parse");
    test_support::expect_eq(
        valid.metadata.embedded_chapters[1].name, std::string {"Main Segment"}, "chapter order should be preserved");

    const auto malformed = ProcessResult {.state = ProcessExitState::Success, .standard_output = "{"};
    test_support::expect_true(
        !parse_probe_output(executable, source, malformed).ok(), "malformed JSON should be rejected");

    const auto missing_fields =
        ProcessResult {.state = ProcessExitState::Success, .standard_output = R"({"format":{}})"};
    test_support::expect_true(
        !parse_probe_output(executable, source, missing_fields).ok(), "missing required fields should be rejected");

    const auto unknown_frame_rate = ProcessResult {
        .state = ProcessExitState::Success,
        .standard_output = R"({"format":{"duration":"1"},"streams":[{"codec_type":"video","avg_frame_rate":"0/0"}]})",
    };
    const ProbeResult unknown_rate = parse_probe_output(executable, source, unknown_frame_rate);
    test_support::expect_true(unknown_rate.ok(), "a valid duration should not require a known frame rate");
    test_support::expect_eq(
        unknown_rate.metadata.duration_ms, u64 {1000}, "duration should survive an unknown frame rate");
    test_support::expect_eq(
        unknown_rate.metadata.frame_rate, FrameRate {}, "an unknown frame rate should use the empty value");

    const auto oversized =
        ProcessResult {.state = ProcessExitState::Success, .standard_output = oversized_stream_output()};
    test_support::expect_true(
        !parse_probe_output(executable, source, oversized).ok(), "unbounded stream metadata should be rejected");

    for (const ProcessExitState state : {ProcessExitState::FailedStart,
             ProcessExitState::TimedOut,
             ProcessExitState::Crashed,
             ProcessExitState::NonzeroExit,
             ProcessExitState::Cancelled}) {
        const ProbeResult failure = parse_probe_output(executable, source, failed(state, "bounded stderr"));
        test_support::expect_true(!failure.ok(), "process failure should reject probing");
        test_support::expect_true(
            contains(failure.error_message, executable.string()), "probe error should include the executable path");
        test_support::expect_true(
            contains(failure.error_message, source.string()), "probe error should include the source path");
        test_support::expect_true(contains(failure.error_message, process_exit_state_name(state)),
            "probe error should identify the exit state");
        test_support::expect_true(
            contains(failure.error_message, "bounded stderr"), "probe error should include bounded stderr context");
    }

    auto observed_request = ProcessRequest {};
    const auto fake = [&observed_request, success_process](const ProcessRequest& request) -> ProcessResult {
        observed_request = request;
        return success_process;
    };
    auto stop_source = std::stop_source {};
    const ProbeResult injected = ProbeService {fake}.probe(executable, source, stop_source.get_token());
    test_support::expect_true(injected.ok(), "injected process executor should support deterministic probing");
    test_support::expect_eq(observed_request.executable, executable, "probe should preserve the executable path");
    test_support::expect_eq(
        observed_request.arguments.back(), source.string(), "probe should preserve the source path");
    test_support::expect_true(
        observed_request.stop_token.stop_possible(), "probe should forward cancellation to the runner");

#ifdef _WIN32
    const auto unicode_source = Path {L"C:\\Temp\\\u89C6\u9891.mkv"};
    const ProbeResult unicode = ProbeService {fake}.probe(executable, unicode_source);
    test_support::expect_true(unicode.ok(), "probing should accept a Unicode source path");
    const auto expected_unicode_argument = std::string {"C:\\Temp\\\xE8\xA7\x86\xE9\xA2\x91.mkv"};
    test_support::expect_eq(observed_request.arguments.back(),
        expected_unicode_argument,
        "the process request should preserve a Windows source path as UTF-8");
#endif

    return 0;
}
