#include "qt/services/export_coordinator.h"

#include "core/command_builder.h"
#include "qt/services/ffprobe_service.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace vidchopper {

ExportCoordinator::ExportCoordinator(QObject* parent)
    : QObject(parent) {
    connect(&process_, &QProcess::readyReadStandardOutput, this, &ExportCoordinator::handle_ready_read_stdout);
    connect(&process_, &QProcess::readyReadStandardError, this, &ExportCoordinator::handle_ready_read_stderr);
    connect(&process_, &QProcess::finished, this, &ExportCoordinator::handle_process_finished);
}

auto ExportCoordinator::busy() const -> bool {
    return busy_;
}

auto ExportCoordinator::start_export(
    const VideoMetadata& metadata,
    std::vector<ChapterSegment> chapters,
    const std::filesystem::path& output_directory,
    const ExportSettings& settings,
    const EncoderEnvironment& environment
) -> void {
    if (busy_) {
        return;
    }

    busy_ = true;
    cancel_requested_ = false;
    stdout_buffer_.clear();
    errors_.clear();
    exports_.clear();
    metadata_ = metadata;
    settings_ = settings;
    output_directory_ = output_directory;
    current_index_ = 0;
    completed_duration_ms_ = 0;
    total_duration_ms_ = 0;

    std::filesystem::create_directories(output_directory_);

    for (auto index = u16 {0}; index < chapters.size(); ++index) {
        const auto output_path = output_path_for(metadata, chapters[index], index, output_directory_, settings);
        auto command = build_ffmpeg_command(metadata, chapters[index], output_path, settings, environment);
        auto arguments = QStringList {};
        for (auto command_index = usize {1}; command_index < command.size(); ++command_index) {
            arguments.push_back(QString::fromStdString(command[command_index]));
        }

        arguments.insert(arguments.size() - 1, "-progress");
        arguments.insert(arguments.size() - 1, "pipe:1");
        arguments.insert(arguments.size() - 1, "-nostats");

        exports_.push_back(PendingExport {
            .chapter_index = index,
            .chapter = chapters[index],
            .output_file = QString::fromStdWString(output_path.wstring()),
            .duration_ms = chapters[index].end_ms - chapters[index].start_ms,
            .program = QString::fromStdString(command.front()),
            .arguments = arguments,
        });

        total_duration_ms_ += chapters[index].end_ms - chapters[index].start_ms;
    }

    emit progress_changed(0);
    start_next();
}

auto ExportCoordinator::cancel() -> void {
    if (!busy_) {
        return;
    }

    cancel_requested_ = true;
    emit log_message("Cancellation requested. Waiting for ffmpeg to stop.");
    process_.kill();
}

auto ExportCoordinator::start_next() -> void {
    if (current_index_ >= exports_.size()) {
        write_manifests();
        busy_ = false;
        emit progress_changed(100);
        emit finished(errors_.isEmpty(), errors_);
        return;
    }

    stdout_buffer_.clear();
    const auto& item = exports_[current_index_];
    emit chapter_started(static_cast<int>(current_index_ + 1), static_cast<int>(exports_.size()), item.output_file);
    emit log_message(QStringLiteral("Exporting %1").arg(item.output_file));

    process_.setProgram(item.program);
    process_.setArguments(item.arguments);
    process_.start();
}

auto ExportCoordinator::handle_ready_read_stdout() -> void {
    stdout_buffer_.append(process_.readAllStandardOutput());

    forever {
        const auto newline_index = stdout_buffer_.indexOf('\n');
        if (newline_index < 0) {
            break;
        }

        const auto line = QString::fromLocal8Bit(stdout_buffer_.first(newline_index)).trimmed();
        stdout_buffer_.remove(0, newline_index + 1);

        if (line.startsWith("out_time_ms=") && current_index_ < exports_.size()) {
            auto ok = false;
            const auto out_time_us = line.mid(QStringLiteral("out_time_ms=").size()).toULongLong(&ok);
            if (!ok) {
                continue;
            }

            const auto chapter_ms = std::min<u64>(out_time_us / 1000, exports_[current_index_].duration_ms);
            const auto overall_ms = completed_duration_ms_ + chapter_ms;
            const auto progress = total_duration_ms_ == 0
                ? 0
                : static_cast<int>((overall_ms * 100) / total_duration_ms_);
            emit progress_changed(progress);
        }
    }
}

auto ExportCoordinator::handle_ready_read_stderr() -> void {
    const auto output = QString::fromLocal8Bit(process_.readAllStandardError()).trimmed();
    if (!output.isEmpty()) {
        emit log_message(output);
    }
}

auto ExportCoordinator::handle_process_finished(const int exit_code, const QProcess::ExitStatus exit_status) -> void {
    if (cancel_requested_) {
        busy_ = false;
        errors_.push_back("Export cancelled.");
        emit finished(false, errors_);
        return;
    }

    if (exit_status != QProcess::NormalExit || exit_code != 0) {
        handle_failure(QStringLiteral("ffmpeg exited with code %1 while exporting %2").arg(exit_code).arg(exports_[current_index_].output_file));
        return;
    }

    if (settings_.verify_output_durations) {
        const auto actual_duration = FfprobeService::probe_duration_ms(
            QString::fromStdString(settings_.ffprobe_path),
            exports_[current_index_].output_file
        );

        if (!actual_duration.has_value()) {
            handle_failure(QStringLiteral("ffprobe could not verify %1 after export.").arg(exports_[current_index_].output_file));
            return;
        }

        const auto expected_duration = exports_[current_index_].duration_ms;
        const auto delta = expected_duration > *actual_duration
            ? expected_duration - *actual_duration
            : *actual_duration - expected_duration;
        if (delta > 1000) {
            handle_failure(QStringLiteral("Duration verification failed for %1.").arg(exports_[current_index_].output_file));
            return;
        }
    }

    completed_duration_ms_ += exports_[current_index_].duration_ms;
    emit progress_changed(total_duration_ms_ == 0 ? 100 : static_cast<int>((completed_duration_ms_ * 100) / total_duration_ms_));
    ++current_index_;
    start_next();
}

auto ExportCoordinator::handle_failure(const QString& message) -> void {
    emit log_message(message);
    errors_.push_back(message);

    if (settings_.stop_on_first_error || current_index_ + 1 >= exports_.size()) {
        busy_ = false;
        emit finished(false, errors_);
        return;
    }

    ++current_index_;
    start_next();
}

auto ExportCoordinator::write_manifests() -> void {
    if (settings_.write_json_manifest) {
        auto chapter_array = QJsonArray {};
        for (const auto& item : exports_) {
            chapter_array.push_back(QJsonObject {
                {"index", static_cast<int>(item.chapter_index + 1)},
                {"name", QString::fromStdString(item.chapter.name)},
                {"startMs", static_cast<qint64>(item.chapter.start_ms)},
                {"endMs", static_cast<qint64>(item.chapter.end_ms)},
                {"outputFile", item.output_file},
            });
        }

        auto root = QJsonObject {
            {"source", QString::fromStdWString(metadata_.source_path.wstring())},
            {"outputDirectory", QString::fromStdWString(output_directory_.wstring())},
            {"chapters", chapter_array},
        };

        auto file = QFile(QString::fromStdWString((output_directory_ / "vidchopper-manifest.json").wstring()));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        }
    }

    if (settings_.write_csv_manifest) {
        auto file = QFile(QString::fromStdWString((output_directory_ / "vidchopper-manifest.csv").wstring()));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return;
        }

        auto stream = QTextStream(&file);
        stream << "index,name,start_ms,end_ms,output_file\n";
        for (const auto& item : exports_) {
            stream << (item.chapter_index + 1)
                   << ",\"" << QString {QString::fromStdString(item.chapter.name)}.replace('"', '\'') << "\""
                   << "," << item.chapter.start_ms
                   << "," << item.chapter.end_ms
                   << ",\"" << QString {item.output_file}.replace('"', '\'') << "\"\n";
        }
    }
}

} // namespace vidchopper
