#include "core/models.hpp"
#include "test_support.hpp"

#include <cmath>
#include <type_traits>

using namespace vidchopper;

// Two-state modes carry their state directly in a bool underlying type. The
// enumerator values stay 0/1 so persisted QSettings/INI values round-trip
// byte-identically with the previous u8-backed definitions.
static_assert(std::is_same_v<std::underlying_type_t<TimestampDisplayMode>, bool>);
static_assert(std::is_same_v<std::underlying_type_t<AudioMode>, bool>);
static_assert(std::is_same_v<std::underlying_type_t<SeekMode>, bool>);
static_assert(static_cast<int>(TimestampDisplayMode::Milliseconds) == 0);
static_assert(static_cast<int>(TimestampDisplayMode::Frames) == 1);
static_assert(static_cast<int>(AudioMode::Copy) == 0);
static_assert(static_cast<int>(AudioMode::Aac) == 1);
static_assert(static_cast<int>(SeekMode::Accurate) == 0);
static_assert(static_cast<int>(SeekMode::Fast) == 1);

// Multi-state modes remain u8-backed.
static_assert(std::is_same_v<std::underlying_type_t<EncoderKind>, u8>);
static_assert(std::is_same_v<std::underlying_type_t<ContainerMode>, u8>);
static_assert(std::is_same_v<std::underlying_type_t<OverwriteMode>, u8>);

// Lock the constexpr/noexcept contract of the FrameRate accessors: these must
// evaluate at compile time, not just at runtime.
static_assert(FrameRate {.numerator = 24, .denominator = 1}.valid());
static_assert(!FrameRate {}.valid());
static_assert(FrameRate {.numerator = 24, .denominator = 1}.as_f64() == 24.0);
static_assert(FrameRate {.numerator = 30000, .denominator = 1001}.display_frames_per_second() == 30);
static_assert(FrameRate {.numerator = 1, .denominator = 4}.display_frames_per_second() == 1);
static_assert(noexcept(FrameRate {}.valid()));
static_assert(FrameRate {.numerator = 24, .denominator = 1} == FrameRate {.numerator = 24, .denominator = 1});
static_assert(FrameRate {.numerator = 24, .denominator = 1} > FrameRate {.numerator = 23, .denominator = 1});

auto main() -> int {
    static_assert(std::is_same_v<std::underlying_type_t<TimestampDisplayMode>, bool>);
    static_assert(std::is_same_v<std::underlying_type_t<AudioMode>, bool>);
    static_assert(std::is_same_v<std::underlying_type_t<SeekMode>, bool>);
    static_assert(static_cast<int>(TimestampDisplayMode::Milliseconds) == 0);
    static_assert(static_cast<int>(TimestampDisplayMode::Frames) == 1);
    static_assert(static_cast<int>(AudioMode::Copy) == 0);
    static_assert(static_cast<int>(AudioMode::Aac) == 1);
    static_assert(static_cast<int>(SeekMode::Accurate) == 0);
    static_assert(static_cast<int>(SeekMode::Fast) == 1);

    // FrameRate::valid()
    {
        const auto valid_rate = FrameRate {.numerator = 24, .denominator = 1};
        test_support::expect_true(valid_rate.valid(), "24/1 should be valid");

        const auto zero_num = FrameRate {.numerator = 0, .denominator = 1};
        test_support::expect_true(!zero_num.valid(), "0/1 should be invalid");

        const auto zero_den = FrameRate {.numerator = 24, .denominator = 0};
        test_support::expect_true(!zero_den.valid(), "24/0 should be invalid");

        const auto both_zero = FrameRate {};
        test_support::expect_true(!both_zero.valid(), "default-constructed should be invalid");
    }

    // FrameRate::as_f64()
    {
        const auto rate_24 = FrameRate {.numerator = 24, .denominator = 1};
        test_support::expect_true(std::abs(rate_24.as_f64() - 24.0) < 0.001, "24/1 should yield 24.0");

        const auto rate_2997 = FrameRate {.numerator = 30000, .denominator = 1001};
        test_support::expect_true(std::abs(rate_2997.as_f64() - 29.97) < 0.01, "30000/1001 should yield ~29.97");

        const auto rate_60 = FrameRate {.numerator = 60, .denominator = 1};
        test_support::expect_true(std::abs(rate_60.as_f64() - 60.0) < 0.001, "60/1 should yield 60.0");

        const auto invalid_rate = FrameRate {.numerator = 0, .denominator = 1};
        test_support::expect_true(invalid_rate.as_f64() == 0.0, "invalid rate should yield 0.0");
    }

    // FrameRate::display_frames_per_second()
    {
        const auto rate_24 = FrameRate {.numerator = 24, .denominator = 1};
        test_support::expect_eq(
            rate_24.display_frames_per_second(), static_cast<u32>(24), "24fps should display as 24");

        const auto rate_2997 = FrameRate {.numerator = 30000, .denominator = 1001};
        test_support::expect_eq(
            rate_2997.display_frames_per_second(), static_cast<u32>(30), "29.97fps should round to 30");

        const auto rate_2398 = FrameRate {.numerator = 24000, .denominator = 1001};
        test_support::expect_eq(
            rate_2398.display_frames_per_second(), static_cast<u32>(24), "23.976fps should round to 24");

        const auto rate_5994 = FrameRate {.numerator = 60000, .denominator = 1001};
        test_support::expect_eq(
            rate_5994.display_frames_per_second(), static_cast<u32>(60), "59.94fps should round to 60");

        const auto rate_25 = FrameRate {.numerator = 25, .denominator = 1};
        test_support::expect_eq(
            rate_25.display_frames_per_second(), static_cast<u32>(25), "25fps should display as 25");

        const auto invalid_rate = FrameRate {};
        test_support::expect_eq(
            invalid_rate.display_frames_per_second(), static_cast<u32>(0), "invalid rate should yield 0");

        const auto very_low = FrameRate {.numerator = 1, .denominator = 4};
        test_support::expect_eq(
            very_low.display_frames_per_second(), static_cast<u32>(1), "sub-1fps should clamp to 1");
    }

    return 0;
}
