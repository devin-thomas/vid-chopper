#include "core/timecode.h"
#include "test_support.h"

using namespace vidchopper;

auto main() -> int {
    // parse_millisecond_timecode: empty and whitespace inputs
    {
        test_support::expect_true(!parse_millisecond_timecode("").has_value(), "empty string should fail");
        test_support::expect_true(!parse_millisecond_timecode("   ").has_value(), "whitespace-only should fail");
    }

    // parse_millisecond_timecode: leading/trailing whitespace is trimmed
    {
        const auto result = parse_millisecond_timecode("  00:01:30.000  ");
        test_support::expect_true(result.has_value(), "timecode with surrounding whitespace should parse");
        test_support::expect_eq(*result, 90000ULL, "whitespace-trimmed parse should yield correct ms");
    }

    // parse_millisecond_timecode: two-segment form (MM:SS.mmm)
    {
        const auto result = parse_millisecond_timecode("05:30.500");
        test_support::expect_true(result.has_value(), "MM:SS.mmm format should parse");
        test_support::expect_eq(*result, 330500ULL, "5 min 30.5 sec = 330500ms");
    }

    // parse_millisecond_timecode: whole seconds without fraction
    {
        const auto result = parse_millisecond_timecode("00:00:10");
        test_support::expect_true(result.has_value(), "timecode without fractional part should parse");
        test_support::expect_eq(*result, 10000ULL, "10 seconds = 10000ms");
    }

    // parse_millisecond_timecode: single-digit fraction (tenths)
    {
        const auto result = parse_millisecond_timecode("00:00:01.5");
        test_support::expect_true(result.has_value(), "single-digit fraction should parse");
        test_support::expect_eq(*result, 1500ULL, "1.5 seconds = 1500ms");
    }

    // parse_millisecond_timecode: two-digit fraction (hundredths)
    {
        const auto result = parse_millisecond_timecode("00:00:02.75");
        test_support::expect_true(result.has_value(), "two-digit fraction should parse");
        test_support::expect_eq(*result, 2750ULL, "2.75 seconds = 2750ms");
    }

    // parse_millisecond_timecode: zero timecode
    {
        const auto result = parse_millisecond_timecode("00:00:00.000");
        test_support::expect_true(result.has_value(), "zero timecode should parse");
        test_support::expect_eq(*result, 0ULL, "zero timecode should be 0ms");
    }

    // parse_millisecond_timecode: large hour value
    {
        const auto result = parse_millisecond_timecode("10:00:00.000");
        test_support::expect_true(result.has_value(), "large hour value should parse");
        test_support::expect_eq(*result, 36000000ULL, "10 hours = 36000000ms");
    }

    // parse_millisecond_timecode: invalid inputs
    {
        test_support::expect_true(!parse_millisecond_timecode("abc").has_value(), "non-numeric should fail");
        test_support::expect_true(!parse_millisecond_timecode("00:00:60.000").has_value(), "seconds >= 60 should fail");
        test_support::expect_true(!parse_millisecond_timecode("00:60:00.000").has_value(), "minutes >= 60 should fail");
        test_support::expect_true(!parse_millisecond_timecode("1:2:3:4:5").has_value(), "too many segments should fail");
        test_support::expect_true(!parse_millisecond_timecode("00:00:10.1234").has_value(), "fraction > 3 digits should fail");
        test_support::expect_true(!parse_millisecond_timecode("00:00:10.").has_value(), "trailing dot with empty fraction should fail");
    }

    // format_millisecond_timecode: edge cases
    {
        test_support::expect_eq(format_millisecond_timecode(0), std::string {"00:00:00.000"}, "zero ms should format correctly");
        test_support::expect_eq(format_millisecond_timecode(999), std::string {"00:00:00.999"}, "sub-second ms should format correctly");
        test_support::expect_eq(format_millisecond_timecode(3661001), std::string {"01:01:01.001"}, "1h1m1s1ms should format correctly");
        test_support::expect_eq(format_millisecond_timecode(86399999), std::string {"23:59:59.999"}, "just under 24h should format correctly");
    }

    // parse_frame_timecode: zero frame rate
    {
        const auto zero_fps = FrameRate {.numerator = 0, .denominator = 1};
        test_support::expect_true(!parse_frame_timecode("00:00:10:00", zero_fps).has_value(), "zero fps should fail");
    }

    // parse_frame_timecode: three-segment form (MM:SS:FF)
    {
        const auto fps_30 = FrameRate {.numerator = 30, .denominator = 1};
        const auto result = parse_frame_timecode("01:30:15", fps_30);
        test_support::expect_true(result.has_value(), "MM:SS:FF format should parse");
        test_support::expect_eq(*result, 90500ULL, "1m30s15f at 30fps = 90500ms");
    }

    // parse_frame_timecode: frame at boundary (fps-1)
    {
        const auto fps_24 = FrameRate {.numerator = 24, .denominator = 1};
        const auto result = parse_frame_timecode("00:00:01:23", fps_24);
        test_support::expect_true(result.has_value(), "frame = fps-1 should be valid");
    }

    // parse_frame_timecode: invalid frame >= fps
    {
        const auto fps_30 = FrameRate {.numerator = 30, .denominator = 1};
        test_support::expect_true(!parse_frame_timecode("00:00:10:30", fps_30).has_value(), "frame == fps should fail");
        test_support::expect_true(!parse_frame_timecode("00:00:10:31", fps_30).has_value(), "frame > fps should fail");
    }

    // parse_frame_timecode: invalid segment counts
    {
        const auto fps_24 = FrameRate {.numerator = 24, .denominator = 1};
        test_support::expect_true(!parse_frame_timecode("00:10", fps_24).has_value(), "too few segments should fail");
        test_support::expect_true(!parse_frame_timecode("1:2:3:4:5", fps_24).has_value(), "too many segments should fail");
    }

    // format_frame_timecode: zero fps
    {
        const auto zero_fps = FrameRate {.numerator = 0, .denominator = 1};
        test_support::expect_eq(format_frame_timecode(5000, zero_fps), std::string {"00:00:00:00"}, "zero fps should return zeroed timecode");
    }

    // format_frame_timecode: round-trip consistency
    {
        const auto fps_30 = FrameRate {.numerator = 30, .denominator = 1};
        const auto formatted = format_frame_timecode(0, fps_30);
        test_support::expect_eq(formatted, std::string {"00:00:00:00"}, "zero ms should format as zero frames");

        const auto formatted2 = format_frame_timecode(60000, fps_30);
        test_support::expect_eq(formatted2, std::string {"00:01:00:00"}, "60000ms should be exactly 1 minute at 30fps");
    }

    // format_frame_timecode: non-integer fps (29.97)
    {
        const auto fps_2997 = FrameRate {.numerator = 30000, .denominator = 1001};
        const auto formatted = format_frame_timecode(1000, fps_2997);
        test_support::expect_eq(formatted, std::string {"00:00:01:00"}, "1000ms at 29.97fps should be 1 second 0 frames");
    }

    return 0;
}
