#include "qt/services/ffprobe_service.hpp"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace vidchopper {

namespace {

struct FfprobeProcessResult {
    bool success {false};
    QByteArray standard_output;
    QString error_message;
};

[[nodiscard]] auto bounded_stderr(QByteArray value) -> QString {
    constexpr auto maximum_bytes = qsizetype {4096};
    const bool truncated = value.size() > maximum_bytes;
    value.truncate(maximum_bytes);
    auto text = QString::fromLocal8Bit(value).trimmed();
    if (truncated) {
        text += "... [truncated]";
    }
    return text;
}

[[nodiscard]] auto run_ffprobe(const QString& ffprobe_path,
    const QString& source_path,
    QStringList arguments,
    const int timeout_ms) -> FfprobeProcessResult {
    arguments.push_back(source_path);
    auto process = QProcess {};
    process.start(ffprobe_path, arguments);

    if (!process.waitForStarted(5000)) {
        const auto description = process.error() == QProcess::FailedToStart ? QStringLiteral("failed to start")
                                                                            : QStringLiteral("did not start");
        return FfprobeProcessResult {
            .error_message = QStringLiteral("ffprobe executable '%1' %2 for source '%3': %4")
                                 .arg(ffprobe_path, description, source_path, process.errorString()),
        };
    }

    if (!process.waitForFinished(timeout_ms)) {
        process.kill();
        process.waitForFinished(1000);
        return FfprobeProcessResult {
            .error_message = QStringLiteral("ffprobe executable '%1' timed out after %2 ms for source '%3'.")
                                 .arg(ffprobe_path)
                                 .arg(timeout_ms)
                                 .arg(source_path),
        };
    }

    if (process.exitStatus() != QProcess::NormalExit) {
        return FfprobeProcessResult {
            .error_message = QStringLiteral("ffprobe executable '%1' crashed while probing source '%2'.")
                                 .arg(ffprobe_path, source_path),
        };
    }
    if (process.exitCode() != 0) {
        auto message = QStringLiteral("ffprobe executable '%1' exited with code %2 for source '%3'")
                           .arg(ffprobe_path)
                           .arg(process.exitCode())
                           .arg(source_path);
        const QString diagnostic = bounded_stderr(process.readAllStandardError());
        if (!diagnostic.isEmpty()) {
            message += ": " + diagnostic;
        }
        return FfprobeProcessResult {.error_message = std::move(message)};
    }

    return FfprobeProcessResult {.success = true, .standard_output = process.readAllStandardOutput()};
}

auto normalize_source_path(const QString& source_path) -> std::filesystem::path {
    auto path = std::filesystem::path {source_path.toStdWString()};
    auto error = std::error_code {};
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }

    path = std::filesystem::absolute(path, error);
    return error ? std::filesystem::path {source_path.toStdWString()} : path.lexically_normal();
}

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
    const FfprobeProcessResult process = run_ffprobe(ffprobe_path,
        source_path,
        {
            "-v",
            "error",
            "-print_format",
            "json",
            "-show_format",
            "-show_streams",
            "-show_chapters",
        },
        10000);
    if (!process.success) {
        return VideoProbeResult {.error_message = process.error_message};
    }

    const auto document = QJsonDocument::fromJson(process.standard_output);
    if (!document.isObject()) {
        return VideoProbeResult {
            .error_message = QStringLiteral("ffprobe executable '%1' returned unusable JSON for source '%2'.")
                                 .arg(ffprobe_path, source_path),
        };
    }

    const auto root = document.object();
    const auto format = root["format"].toObject();
    const auto streams = root["streams"].toArray();
    const auto chapters = root["chapters"].toArray();

    auto metadata = VideoMetadata {};
    metadata.source_path = normalize_source_path(source_path);
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

    metadata.embedded_chapters.reserve(static_cast<size_t>(chapters.size()));
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

    std::erase_if(
        metadata.embedded_chapters, [](const ChapterSegment& chapter) { return chapter.end_ms <= chapter.start_ms; });

    return VideoProbeResult {
        .success = metadata.duration_ms > 0,
        .error_message = metadata.duration_ms > 0
            ? QString {}
            : QStringLiteral("ffprobe executable '%1' returned unusable duration metadata for source '%2'.")
                  .arg(ffprobe_path, source_path),
        .metadata = std::move(metadata),
    };
}

auto FfprobeService::probe_duration_ms(const QString& ffprobe_path, const QString& source_path) -> DurationProbeResult {
    const FfprobeProcessResult process = run_ffprobe(ffprobe_path,
        source_path,
        {
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=nokey=1:noprint_wrappers=1",
        },
        5000);
    if (!process.success) {
        return DurationProbeResult {.error_message = process.error_message};
    }

    const auto duration = seconds_string_to_ms(QString::fromLocal8Bit(process.standard_output).trimmed());
    if (!duration.has_value()) {
        return DurationProbeResult {
            .error_message = QStringLiteral("ffprobe executable '%1' returned an unusable duration for source '%2'.")
                                 .arg(ffprobe_path, source_path),
        };
    }
    return DurationProbeResult {.duration_ms = duration};
}

} // namespace vidchopper
