#include "core/timecode.hpp"
#include "test_support.hpp"

using namespace vidchopper;

auto main() -> int {
    test_support::expect_true(
        parse_millisecond_timecode("00:01:05.250").has_value(), "millisecond timecode should parse");
    test_support::expect_eq(
        *parse_millisecond_timecode("00:01:05.250"), 65250ULL, "millisecond parse should preserve milliseconds");
    test_support::expect_eq(format_millisecond_timecode(65250),
        std::string {"00:01:05.250"},
        "millisecond formatter should preserve value");

    const auto frame_rate = FrameRate {.numerator = 24, .denominator = 1};
    test_support::expect_true(
        parse_frame_timecode("00:00:10:12", frame_rate).has_value(), "frame timecode should parse");
    test_support::expect_eq(
        *parse_frame_timecode("00:00:10:12", frame_rate), 10500ULL, "frame parser should resolve to milliseconds");
    test_support::expect_eq(
        format_frame_timecode(10500, frame_rate), std::string {"00:00:10:12"}, "frame formatter should round-trip");

    test_support::expect_true(
        !parse_millisecond_timecode("00:65:10.000").has_value(), "invalid minute segment should fail");
    test_support::expect_true(
        !parse_frame_timecode("00:00:10:24", frame_rate).has_value(), "frame component should be less than fps");

    return 0;
}
