#pragma once

#include "core/types.hpp"

#include <string>
#include <string_view>

namespace vidchopper {

struct ReadyMarkerWriteResult {
    bool success {false};
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

[[nodiscard]] auto write_ready_marker(const Path& target, std::string_view status) -> ReadyMarkerWriteResult;

} // namespace vidchopper
