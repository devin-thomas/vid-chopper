#include "core/enum_utils.h"
#include "core/models.h"
#include "test_support.h"

using namespace vidchopper;

namespace {

// Lock the exact behavior shared by the former clamp_enum / safe_enum_cast
// helpers: in-range values map straight through, everything else falls back.
static_assert(clamp_to_enum(0, EncoderKind::HevcNvenc, EncoderKind::Auto) == EncoderKind::Auto);
static_assert(clamp_to_enum(1, EncoderKind::HevcNvenc, EncoderKind::Auto) == EncoderKind::X264);
static_assert(clamp_to_enum(2, EncoderKind::HevcNvenc, EncoderKind::Auto) == EncoderKind::HevcNvenc);
static_assert(clamp_to_enum(3, EncoderKind::HevcNvenc, EncoderKind::Auto) == EncoderKind::Auto);
static_assert(clamp_to_enum(-1, EncoderKind::HevcNvenc, EncoderKind::Auto) == EncoderKind::Auto);

static_assert(clamp_to_enum(0, AudioMode::Aac, AudioMode::Copy) == AudioMode::Copy);
static_assert(clamp_to_enum(1, AudioMode::Aac, AudioMode::Copy) == AudioMode::Aac);
static_assert(clamp_to_enum(2, AudioMode::Aac, AudioMode::Copy) == AudioMode::Copy);

// The concept only admits scoped enumerations.
static_assert(ScopedEnum<EncoderKind>);
static_assert(!ScopedEnum<int>);

enum LegacyUnscoped {
    LegacyA,
    LegacyB
};
static_assert(!ScopedEnum<LegacyUnscoped>);

} // namespace

auto main() -> int {
    // EncoderKind: 0..2 valid, fallback Auto.
    test_support::expect_true(clamp_to_enum(-5, EncoderKind::HevcNvenc, EncoderKind::Auto) == EncoderKind::Auto,
        "negative raw should fall back to Auto");
    test_support::expect_true(clamp_to_enum(2, EncoderKind::HevcNvenc, EncoderKind::Auto) == EncoderKind::HevcNvenc,
        "raw == max_valid should map to that enumerator");
    test_support::expect_true(clamp_to_enum(99, EncoderKind::HevcNvenc, EncoderKind::Auto) == EncoderKind::Auto,
        "raw beyond max should fall back to Auto");

    // ContainerMode: 0..2 valid, fallback Source.
    test_support::expect_true(
        clamp_to_enum(1, ContainerMode::Mkv, ContainerMode::Source) == ContainerMode::Mp4, "raw 1 should map to Mp4");
    test_support::expect_true(clamp_to_enum(3, ContainerMode::Mkv, ContainerMode::Source) == ContainerMode::Source,
        "raw 3 should fall back to Source");

    // SeekMode: 0..1 valid, fallback Accurate.
    test_support::expect_true(
        clamp_to_enum(1, SeekMode::Fast, SeekMode::Accurate) == SeekMode::Fast, "raw 1 should map to Fast");
    test_support::expect_true(clamp_to_enum(2, SeekMode::Fast, SeekMode::Accurate) == SeekMode::Accurate,
        "raw 2 should fall back to Accurate");

    return 0;
}
