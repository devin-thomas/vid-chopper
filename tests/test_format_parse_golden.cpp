#include "core/string_utils.h"
#include "core/timecode.h"
#include "test_support.h"

using namespace vidchopper;

// Golden tests locking byte-identical output across the std::format /
// std::from_chars modernization. These pin behaviour that the ostringstream
// and hand-rolled digit-loop implementations exhibited at the boundaries.
auto main() -> int {
    // format_millisecond_timecode: exact padding across magnitudes.
    {
        test_support::expect_eq(format_millisecond_timecode(0), std::string {"00:00:00.000"}, "ms zero");
        test_support::expect_eq(format_millisecond_timecode(1), std::string {"00:00:00.001"}, "ms one");
        test_support::expect_eq(format_millisecond_timecode(7000), std::string {"00:00:07.000"}, "ms seven seconds");
        test_support::expect_eq(format_millisecond_timecode(65250), std::string {"00:01:05.250"}, "ms one minute five seconds");
        test_support::expect_eq(format_millisecond_timecode(3661001), std::string {"01:01:01.001"}, "ms hour minute second milli");
        test_support::expect_eq(format_millisecond_timecode(360000000), std::string {"100:00:00.000"}, "ms hours overflow two -> three digits");
    }

    // format_frame_timecode: exact padding and frame component.
    {
        const auto fps_24 = FrameRate {.numerator = 24, .denominator = 1};
        test_support::expect_eq(format_frame_timecode(0, fps_24), std::string {"00:00:00:00"}, "frame zero");
        test_support::expect_eq(format_frame_timecode(10500, fps_24), std::string {"00:00:10:12"}, "frame round-trip");

        const auto fps_30 = FrameRate {.numerator = 30, .denominator = 1};
        test_support::expect_eq(format_frame_timecode(360000000, fps_30), std::string {"100:00:00:00"}, "frame hours three digits");
    }

    // zero_padded_index: width zero, width one, exact width, and max u16.
    {
        test_support::expect_eq(zero_padded_index(42, 0), std::string {"42"}, "width 0 yields bare number");
        test_support::expect_eq(zero_padded_index(0, 0), std::string {"0"}, "zero value width 0");
        test_support::expect_eq(zero_padded_index(5, 1), std::string {"5"}, "width 1 value 5");
        test_support::expect_eq(zero_padded_index(65535, 6), std::string {"065535"}, "max u16 padded to 6");
        test_support::expect_eq(zero_padded_index(65535, 3), std::string {"65535"}, "value wider than width stays intact");
    }

    // parse_millisecond_timecode: from_chars overflow and sign/space rejection.
    {
        test_support::expect_true(!parse_millisecond_timecode("99999999999999999999:00:00.000").has_value(), "hour segment overflow should fail");
        test_support::expect_true(!parse_millisecond_timecode("00:+5:00.000").has_value(), "plus sign should fail");
        test_support::expect_true(!parse_millisecond_timecode("00:-5:00.000").has_value(), "minus sign should fail");
        test_support::expect_true(!parse_millisecond_timecode("00: 5:00.000").has_value(), "interior space should fail");
    }

    // parse_millisecond_timecode: leading zeros are accepted as decimal.
    {
        const auto result = parse_millisecond_timecode("007:00:00.000");
        test_support::expect_true(result.has_value(), "leading-zero hours should parse as decimal");
        test_support::expect_eq(*result, 25200000ULL, "007 hours = 7 hours");
    }

    return 0;
}
