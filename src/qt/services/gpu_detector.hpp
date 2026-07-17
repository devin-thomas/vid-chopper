#pragma once

#include "core/models.hpp"

#include <QString>

namespace vidchopper {

class GpuDetector final {
public:
    [[nodiscard]] static auto detect(const QString& ffmpeg_path) -> EncoderEnvironment;
};

} // namespace vidchopper
