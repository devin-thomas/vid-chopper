#include "qt/ui/zoom_policy.hpp"

#include <algorithm>
#include <limits>

namespace vidchopper {

auto ZoomPolicy::for_screen_height(const int logical_height) noexcept -> int {
    if (logical_height <= 0) {
        return default_percent;
    }

    constexpr auto baseline_height = i64 {1080};
    const i64 scaled = (static_cast<i64>(logical_height) * 100 + (baseline_height / 2)) / baseline_height;
    const int bounded = static_cast<int>((std::min)(scaled, static_cast<i64>(std::numeric_limits<int>::max())));
    return clamp(bounded);
}

} // namespace vidchopper
