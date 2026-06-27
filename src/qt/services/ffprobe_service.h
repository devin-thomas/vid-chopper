#pragma once

#include "core/models.h"

#include <QMetaType>
#include <QString>

#include <optional>

namespace vidchopper {

struct VideoProbeResult {
    bool success {false};
    QString error_message;
    VideoMetadata metadata;
};

class FfprobeService final {
public:
    [[nodiscard]] static auto probe_video(const QString& ffprobe_path, const QString& source_path) -> VideoProbeResult;
    [[nodiscard]] static auto probe_duration_ms(
        const QString& ffprobe_path, const QString& source_path) -> std::optional<u64>;
};

} // namespace vidchopper

Q_DECLARE_METATYPE(vidchopper::VideoProbeResult)
