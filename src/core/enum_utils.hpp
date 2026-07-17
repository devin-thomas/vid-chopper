#pragma once

#include <concepts>
#include <type_traits>

namespace vidchopper {

// A scoped enumeration (enum class): an enum that does not implicitly convert
// to its underlying integer type.
template <typename E>
concept ScopedEnum = std::is_enum_v<E> && !std::convertible_to<E, std::underlying_type_t<E>>;

// Clamp an externally supplied integer (a persisted QSettings value or a combo
// box index) to a valid enumerator. Values outside [0, max_valid] fall back to
// `fallback`, which keeps deserialization total and tamper-proof.
template <ScopedEnum E>
[[nodiscard]] constexpr auto clamp_to_enum(int raw, E max_valid, E fallback) -> E {
    if (raw < 0 || raw > static_cast<int>(max_valid)) {
        return fallback;
    }
    return static_cast<E>(raw);
}

} // namespace vidchopper
