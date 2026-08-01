#pragma once

#include "core/models.hpp"

#include <QMetaType>
#include <QString>

#include <optional>

namespace vidchopper {

struct VideoProbeResult {
    bool success {false};
    QString error_message;
    VideoMetadata metadata;
};

struct DurationProbeResult {
    std::optional<u64> duration_ms;
    QString error_message;

    [[nodiscard]] auto ok() const noexcept -> bool {
        return duration_ms.has_value() && error_message.isEmpty();
    }
};

class FfprobeService final {
public:
    [[nodiscard]] static auto probe_video(const QString& ffprobe_path, const QString& source_path) -> VideoProbeResult;
    [[nodiscard]] static auto probe_duration_ms(
        const QString& ffprobe_path, const QString& source_path) -> DurationProbeResult;
};

} // namespace vidchopper

Q_DECLARE_METATYPE(vidchopper::VideoProbeResult)
