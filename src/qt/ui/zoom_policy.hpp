#pragma once

#include "core/types.hpp"

#include <algorithm>
#include <array>

namespace vidchopper {

struct ZoomPolicy final {
    static constexpr auto minimum_percent = 50;
    static constexpr auto maximum_percent = 300;
    static constexpr auto step_percent = 25;
    static constexpr auto default_percent = 100;
    static constexpr auto preset_count = size_t {(maximum_percent - minimum_percent) / step_percent + 1};

    [[nodiscard]] static constexpr auto clamp(const int zoom_percent) noexcept -> int {
        const int clamped = std::clamp(zoom_percent, minimum_percent, maximum_percent);
        const int steps = (clamped - minimum_percent + (step_percent / 2)) / step_percent;
        return minimum_percent + (steps * step_percent);
    }

    [[nodiscard]] static constexpr auto presets() noexcept -> std::array<int, preset_count> {
        auto values = std::array<int, preset_count> {};
        for (auto index = size_t {0}; index < values.size(); ++index) {
            values[index] = minimum_percent + (static_cast<int>(index) * step_percent);
        }
        return values;
    }

    [[nodiscard]] static auto for_screen_height(int logical_height) noexcept -> int;
};

} // namespace vidchopper
