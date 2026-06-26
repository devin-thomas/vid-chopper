#include "qt/services/ffprobe_service.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <algorithm>
#include <optional>

namespace vidchopper {

namespace {

auto rational_from_string(const QString& value) -> FrameRate {
    const auto parts = value.split('/');
    if (parts.size() != 2) {
        return {};
    }

    auto numerator_ok = false;
    auto denominator_ok = false;
    const auto numerator = parts[0].toUInt(&numerator_ok);
    const auto denominator = parts[1].toUInt(&denominator_ok);

    if (!numerator_ok || !denominator_ok || denominator == 0) {
        return {};
    }

    return FrameRate {.numerator = numerator, .denominator = denominator};
}

auto seconds_string_to_ms(const QString& value) -> std::optional<u64> {
    auto ok = false;
    const auto seconds = value.toDouble(&ok);
    if (!ok || seconds < 0.0) {
        return std::nullopt;
    }

    return static_cast<u64>((seconds * 1000.0) + 0.5);
}

} // namespace

auto FfprobeService::probe_video(const QString& ffprobe_path, const QString& source_path) -> VideoProbeResult {
    auto process = QProcess {};
    process.start(
        ffprobe_path,
        {
            "-v",
            "error",
            "-print_format",
            "json",
            "-show_format",
            "-show_streams",
            "-show_chapters",
            source_path,
        }
    );

    if (!process.waitForFinished(10000)) {
        return VideoProbeResult {
            .error_message = "ffprobe timed out while probing the selected video.",
        };
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return VideoProbeResult {
            .error_message = QString::fromLocal8Bit(process.readAllStandardError()).trimmed(),
        };
    }

    const auto document = QJsonDocument::fromJson(process.readAllStandardOutput());
    if (!document.isObject()) {
        return VideoProbeResult {
            .error_message = "ffprobe returned malformed JSON output.",
        };
    }

    const auto root = document.object();
    const auto format = root["format"].toObject();
    const auto streams = root["streams"].toArray();
    const auto chapters = root["chapters"].toArray();

    auto metadata = VideoMetadata {};
    metadata.source_path = std::filesystem::path(source_path.toStdWString());
    const auto parsed_duration = seconds_string_to_ms(format["duration"].toString());
    metadata.duration_ms = parsed_duration.value_or(0);

    for (const auto stream_value : streams) {
        const auto stream = stream_value.toObject();
        if (stream["codec_type"].toString() != "video") {
            continue;
        }

        metadata.frame_rate = rational_from_string(stream["avg_frame_rate"].toString());
        if (!metadata.frame_rate.valid()) {
            metadata.frame_rate = rational_from_string(stream["r_frame_rate"].toString());
        }
        break;
    }

    metadata.source_extension = QFileInfo(source_path).suffix().isEmpty()
        ? std::string {".mp4"}
        : "." + QFileInfo(source_path).suffix().toLower().toStdString();

    metadata.embedded_chapters.reserve(static_cast<usize>(chapters.size()));
    for (auto index = 0; index < chapters.size(); ++index) {
        const auto chapter = chapters[index].toObject();
        const auto tags = chapter["tags"].toObject();
        const auto title = tags["title"].toString(QStringLiteral("Chapter %1").arg(index + 1));
        const auto start = seconds_string_to_ms(chapter["start_time"].toString());
        const auto end = seconds_string_to_ms(chapter["end_time"].toString());
        if (!start.has_value() || !end.has_value()) {
            continue;
        }
        metadata.embedded_chapters.push_back(ChapterSegment {
            .name = title.toStdString(),
            .start_ms = *start,
            .end_ms = *end,
        });
    }

    metadata.embedded_chapters.erase(
        std::remove_if(
            metadata.embedded_chapters.begin(),
            metadata.embedded_chapters.end(),
            [](const ChapterSegment& chapter) {
                return chapter.end_ms <= chapter.start_ms;
            }
        ),
        metadata.embedded_chapters.end()
    );

    return VideoProbeResult {
        .success = metadata.duration_ms > 0,
        .error_message = metadata.duration_ms > 0 ? QString {} : "The selected file did not report a valid duration.",
        .metadata = std::move(metadata),
    };
}

auto FfprobeService::probe_duration_ms(const QString& ffprobe_path, const QString& source_path) -> std::optional<u64> {
    auto process = QProcess {};
    process.start(
        ffprobe_path,
        {
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=nokey=1:noprint_wrappers=1",
            source_path,
        }
    );

    if (!process.waitForFinished(5000)) {
        return std::nullopt;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return std::nullopt;
    }

    return seconds_string_to_ms(QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed());
}

} // namespace vidchopper
