#include "core/ready_marker.hpp"

#include "core/path_utils.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace vidchopper {

auto ReadyMarkerWriteResult::ok() const noexcept -> bool {
    return success && error_message.empty();
}

auto write_ready_marker(const Path& target, const std::string_view status) -> ReadyMarkerWriteResult {
    if (target.empty()) {
        return ReadyMarkerWriteResult {.error_message = "Ready-marker path is empty."};
    }

    auto error = std::error_code {};
    const Path parent = target.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return ReadyMarkerWriteResult {
                .error_message =
                    "Could not create ready-marker directory '" + path_to_utf8(parent) + "': " + error.message(),
            };
        }
    }

    Path temporary = target;
    temporary += ".tmp";
    auto stream = std::ofstream {temporary, std::ios::binary | std::ios::trunc};
    if (!stream.is_open()) {
        return ReadyMarkerWriteResult {
            .error_message = "Could not open ready-marker temporary file '" + path_to_utf8(temporary) + "'.",
        };
    }

    stream.write(status.data(), static_cast<std::streamsize>(status.size()));
    stream.put('\n');
    stream.flush();
    if (!stream.good()) {
        stream.close();
        std::filesystem::remove(temporary, error);
        return ReadyMarkerWriteResult {
            .error_message =
                "Could not completely write ready-marker temporary file '" + path_to_utf8(temporary) + "'.",
        };
    }
    stream.close();
    if (stream.fail()) {
        std::filesystem::remove(temporary, error);
        return ReadyMarkerWriteResult {
            .error_message = "Could not close ready-marker temporary file '" + path_to_utf8(temporary) + "'.",
        };
    }

    std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
    if (error) {
        auto cleanup_error = std::error_code {};
        std::filesystem::remove(temporary, cleanup_error);
        return ReadyMarkerWriteResult {
            .error_message = "Could not publish ready marker '" + path_to_utf8(target) + "': " + error.message(),
        };
    }

    return ReadyMarkerWriteResult {.success = true};
}

} // namespace vidchopper
