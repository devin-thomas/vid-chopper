#pragma once

namespace vidchopper {

[[nodiscard]] constexpr auto can_start_export(const bool probe_busy, const bool has_metadata) noexcept -> bool {
    return !probe_busy && has_metadata;
}

} // namespace vidchopper
