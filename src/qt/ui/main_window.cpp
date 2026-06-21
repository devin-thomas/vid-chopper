#include "qt/ui/main_window.h"

#include "core/chapter_plan.h"
#include "core/command_builder.h"
#include "core/timecode.h"
#include "qt/app_settings.h"
#include "qt/services/export_coordinator.h"
#include "qt/services/ffprobe_service.h"
#include "qt/services/gpu_detector.h"
#include "qt/ui/advanced_settings_dialog.h"
#include "qt/ui/chapter_table_model.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

namespace vidchopper {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , settings_store_(new QSettings {this})
    , settings_(load_export_settings(*settings_store_))
    , chapter_model_(new ChapterTableModel {this})
    , export_coordinator_(new ExportCoordinator {this}) {
    setWindowTitle("VidChopper");
    resize(1280, 860);

    build_ui();
    create_menus();
    apply_settings_to_ui();
    redetect_gpu();

    connect(chapter_model_, &ChapterTableModel::chapters_changed, this, [this]() {
        chapter_count_spin_->blockSignals(true);
        chapter_count_spin_->setValue(chapter_model_->rowCount());
        chapter_count_spin_->blockSignals(false);
    });

    connect(export_coordinator_, &ExportCoordinator::log_message, this, &MainWindow::append_log_message);
    connect(export_coordinator_, &ExportCoordinator::progress_changed, progress_bar_, &QProgressBar::setValue);
    connect(export_coordinator_, &ExportCoordinator::chapter_started, this, [this](const int current, const int total, const QString& output_file) {
        statusBar()->showMessage(QStringLiteral("Exporting chapter %1 of %2").arg(current).arg(total));
        append_log_message(QStringLiteral("Writing %1").arg(output_file));
    });
    connect(export_coordinator_, &ExportCoordinator::finished, this, &MainWindow::handle_export_finished);
}

auto MainWindow::create_menus() -> void {
    auto* file_menu = menuBar()->addMenu("&File");
    file_menu->addAction("&Open Video...", this, &MainWindow::open_video, QKeySequence::Open);
    file_menu->addAction("Choose &Output Directory...", this, &MainWindow::choose_output_directory);
    file_menu->addSeparator();
    file_menu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);

    auto* advanced_menu = menuBar()->addMenu("&Advanced");
    advanced_menu->addAction("&Settings...", this, &MainWindow::open_advanced_settings);
    advanced_menu->addAction("&Re-detect GPU", this, &MainWindow::redetect_gpu);
    advanced_menu->addAction("&Reset Output Directory", this, &MainWindow::reset_output_directory);
    auto* open_output_folder_action = new QAction {"Open Output &Folder", this};
    connect(open_output_folder_action, &QAction::triggered, this, [this]() {
        if (!output_directory_edit_->text().isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(output_directory_edit_->text()));
        }
    });
    advanced_menu->addAction(open_output_folder_action);

    auto* help_menu = menuBar()->addMenu("&Help");
    auto* about_vidchopper_action = new QAction {"&About VidChopper", this};
    connect(about_vidchopper_action, &QAction::triggered, this, [this]() {
        QMessageBox::about(
            this,
            "About VidChopper",
            "VidChopper is a Windows-first Qt desktop application for turning one source video into chapter clips with ffmpeg."
        );
    });
    help_menu->addAction(about_vidchopper_action);

    auto* about_qt_action = new QAction {"About &Qt", this};
    connect(about_qt_action, &QAction::triggered, qApp, &QApplication::aboutQt);
    help_menu->addAction(about_qt_action);
}

auto MainWindow::build_ui() -> void {
    auto* central = new QWidget {this};
    auto* root_layout = new QVBoxLayout {central};

    auto* source_group = new QGroupBox {"Source and Output", central};
    auto* source_layout = new QGridLayout {source_group};

    source_path_edit_ = new QLineEdit {source_group};
    source_path_edit_->setReadOnly(true);
    auto* open_button = new QPushButton {"Open Video...", source_group};
    connect(open_button, &QPushButton::clicked, this, &MainWindow::open_video);

    output_directory_edit_ = new QLineEdit {source_group};
    output_directory_edit_->setReadOnly(true);
    auto* browse_output_button = new QPushButton {"Choose Folder...", source_group};
    connect(browse_output_button, &QPushButton::clicked, this, &MainWindow::choose_output_directory);

    chapter_count_spin_ = new QSpinBox {source_group};
    chapter_count_spin_->setRange(1, 255);
    chapter_count_spin_->setValue(settings_.default_chapter_count);

    auto* distribute_button = new QPushButton {"Redistribute Evenly", source_group};
    connect(distribute_button, &QPushButton::clicked, this, &MainWindow::redistribute_chapters);
    auto* import_button = new QPushButton {"Import Embedded Chapters", source_group};
    connect(import_button, &QPushButton::clicked, this, &MainWindow::import_embedded_chapters);
    auto* add_button = new QPushButton {"Add Chapter", source_group};
    connect(add_button, &QPushButton::clicked, this, &MainWindow::add_chapter);
    auto* remove_button = new QPushButton {"Remove Selected", source_group};
    connect(remove_button, &QPushButton::clicked, this, &MainWindow::remove_selected_chapters);

    source_layout->addWidget(new QLabel {"Source file"}, 0, 0);
    source_layout->addWidget(source_path_edit_, 0, 1);
    source_layout->addWidget(open_button, 0, 2);
    source_layout->addWidget(new QLabel {"Output folder"}, 1, 0);
    source_layout->addWidget(output_directory_edit_, 1, 1);
    source_layout->addWidget(browse_output_button, 1, 2);
    source_layout->addWidget(new QLabel {"Planned chapter count"}, 2, 0);
    source_layout->addWidget(chapter_count_spin_, 2, 1);
    source_layout->addWidget(distribute_button, 2, 2);
    source_layout->addWidget(import_button, 3, 0);
    source_layout->addWidget(add_button, 3, 1);
    source_layout->addWidget(remove_button, 3, 2);

    auto* summary_group = new QGroupBox {"Session Summary", central};
    auto* summary_layout = new QGridLayout {summary_group};
    duration_value_label_ = new QLabel {"-", summary_group};
    frame_rate_value_label_ = new QLabel {"-", summary_group};
    chapter_source_value_label_ = new QLabel {"No video loaded", summary_group};
    encoder_value_label_ = new QLabel {"Detecting...", summary_group};

    summary_layout->addWidget(new QLabel {"Duration"}, 0, 0);
    summary_layout->addWidget(duration_value_label_, 0, 1);
    summary_layout->addWidget(new QLabel {"Frame rate"}, 0, 2);
    summary_layout->addWidget(frame_rate_value_label_, 0, 3);
    summary_layout->addWidget(new QLabel {"Chapter source"}, 1, 0);
    summary_layout->addWidget(chapter_source_value_label_, 1, 1, 1, 3);
    summary_layout->addWidget(new QLabel {"Encoder path"}, 2, 0);
    summary_layout->addWidget(encoder_value_label_, 2, 1, 1, 3);

    chapter_table_ = new QTableView {central};
    chapter_table_->setModel(chapter_model_);
    chapter_table_->horizontalHeader()->setStretchLastSection(true);
    chapter_table_->verticalHeader()->setVisible(false);
    chapter_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    chapter_table_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto* export_row = new QWidget {central};
    auto* export_layout = new QHBoxLayout {export_row};
    progress_bar_ = new QProgressBar {export_row};
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    export_button_ = new QPushButton {"Export Chapters", export_row};
    connect(export_button_, &QPushButton::clicked, this, &MainWindow::start_or_cancel_export);
    export_layout->addWidget(progress_bar_, 1);
    export_layout->addWidget(export_button_);

    log_output_ = new QPlainTextEdit {central};
    log_output_->setReadOnly(true);
    log_output_->setPlaceholderText("ffmpeg and ffprobe activity will appear here.");

    root_layout->addWidget(source_group);
    root_layout->addWidget(summary_group);
    root_layout->addWidget(chapter_table_, 1);
    root_layout->addWidget(export_row);
    root_layout->addWidget(log_output_, 1);

    setCentralWidget(central);
    statusBar()->showMessage("Ready");
}

auto MainWindow::open_video() -> void {
    const auto file_path = QFileDialog::getOpenFileName(
        this,
        "Select a source video",
        QString {},
        "Video Files (*.mp4 *.mkv *.mov *.m4v *.avi *.webm);;All Files (*.*)"
    );

    if (!file_path.isEmpty()) {
        load_video(file_path);
    }
}

auto MainWindow::choose_output_directory() -> void {
    const auto directory = QFileDialog::getExistingDirectory(this, "Choose output directory", output_directory_edit_->text());
    if (!directory.isEmpty()) {
        output_directory_edit_->setText(directory);
        output_directory_overridden_ = true;
    }
}

auto MainWindow::reset_output_directory() -> void {
    if (!metadata_.has_value()) {
        return;
    }

    output_directory_edit_->setText(QString::fromStdWString(default_output_directory(metadata_->source_path, settings_).wstring()));
    output_directory_overridden_ = false;
}

auto MainWindow::import_embedded_chapters() -> void {
    if (!metadata_.has_value()) {
        return;
    }

    if (metadata_->embedded_chapters.empty()) {
        QMessageBox::information(this, "No embedded chapters", "The selected video does not contain chapter metadata.");
        return;
    }

    chapter_model_->set_chapters(metadata_->embedded_chapters);
    chapter_source_value_label_->setText("Embedded chapter metadata");
}

auto MainWindow::redistribute_chapters() -> void {
    if (!metadata_.has_value()) {
        return;
    }

    chapter_model_->set_chapters(build_default_chapters(metadata_->duration_ms, static_cast<u8>(chapter_count_spin_->value())));
    chapter_source_value_label_->setText("Evenly distributed from the current chapter count");
}

auto MainWindow::add_chapter() -> void {
    if (!metadata_.has_value()) {
        return;
    }

    if (!chapter_model_->append_chapter(metadata_->duration_ms)) {
        QMessageBox::warning(this, "Cannot add chapter", "VidChopper could not split the current layout into another chapter while keeping every segment at least one second long.");
    }
}

auto MainWindow::remove_selected_chapters() -> void {
    chapter_model_->remove_rows(chapter_table_->selectionModel()->selectedRows());
}

auto MainWindow::open_advanced_settings() -> void {
    auto dialog = AdvancedSettingsDialog {this};
    dialog.set_settings(settings_);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    settings_ = dialog.settings();
    save_export_settings(*settings_store_, settings_);
    apply_settings_to_ui();
    if (!output_directory_overridden_ && metadata_.has_value()) {
        reset_output_directory();
    }
    refresh_summary();
}

auto MainWindow::redetect_gpu() -> void {
    environment_ = GpuDetector::detect(QString::fromStdString(settings_.ffmpeg_path));
    refresh_summary();
    append_log_message(resolve_encoder_summary());
}

auto MainWindow::start_or_cancel_export() -> void {
    if (export_coordinator_->busy()) {
        export_coordinator_->cancel();
        return;
    }

    if (!metadata_.has_value()) {
        QMessageBox::warning(this, "No source loaded", "Select a source video before exporting chapters.");
        return;
    }

    const auto chapters = chapter_model_->chapters();
    const auto validation = validate_chapters(chapters, metadata_->duration_ms, settings_);
    if (!validation.ok()) {
        auto details = QStringList {};
        for (const auto& issue : validation.issues) {
            details << QString::fromStdString(issue.message);
        }

        QMessageBox::warning(this, "Invalid chapter plan", details.join('\n'));
        return;
    }

    export_button_->setText("Cancel Export");
    progress_bar_->setValue(0);
    append_log_message("Starting export.");

    export_coordinator_->start_export(
        *metadata_,
        chapters,
        current_output_directory(),
        settings_,
        environment_
    );
}

auto MainWindow::handle_export_finished(const bool success, const QStringList& errors) -> void {
    export_button_->setText("Export Chapters");
    statusBar()->showMessage(success ? "Export complete" : "Export ended with errors");

    if (!errors.isEmpty()) {
        append_log_message(errors.join('\n'));
    }

    if (success && settings_.open_output_directory_after_export) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(output_directory_edit_->text()));
    }
}

auto MainWindow::load_video(const QString& source_path) -> void {
    const auto probe_result = FfprobeService::probe_video(QString::fromStdString(settings_.ffprobe_path), source_path);
    if (!probe_result.success) {
        QMessageBox::critical(this, "ffprobe error", probe_result.error_message);
        append_log_message(probe_result.error_message);
        return;
    }

    metadata_ = probe_result.metadata;
    source_path_edit_->setText(source_path);

    if (settings_.prefer_embedded_chapters && !metadata_->embedded_chapters.empty()) {
        chapter_model_->set_chapters(metadata_->embedded_chapters);
        chapter_source_value_label_->setText("Embedded chapter metadata");
    } else {
        chapter_model_->set_chapters(build_default_chapters(metadata_->duration_ms, settings_.default_chapter_count));
        chapter_source_value_label_->setText("Evenly distributed starter layout");
    }

    chapter_count_spin_->setValue(chapter_model_->rowCount());
    chapter_model_->set_frame_rate(metadata_->frame_rate);

    if (!output_directory_overridden_) {
        output_directory_edit_->setText(QString::fromStdWString(default_output_directory(metadata_->source_path, settings_).wstring()));
    }

    refresh_summary();
    append_log_message(QStringLiteral("Loaded %1").arg(source_path));
}

auto MainWindow::apply_settings_to_ui() -> void {
    chapter_count_spin_->setValue(settings_.default_chapter_count);
    chapter_model_->set_display_mode(settings_.display_mode);
}

auto MainWindow::refresh_summary() -> void {
    if (metadata_.has_value()) {
        duration_value_label_->setText(QString::fromStdString(format_millisecond_timecode(metadata_->duration_ms)));
        const auto fps = metadata_->frame_rate.as_f64();
        frame_rate_value_label_->setText(fps > 0.0 ? QString::number(fps, 'f', 3) + " fps" : "Unknown");
    } else {
        duration_value_label_->setText("-");
        frame_rate_value_label_->setText("-");
        chapter_source_value_label_->setText("No video loaded");
    }

    encoder_value_label_->setText(resolve_encoder_summary());
}

auto MainWindow::append_log_message(const QString& message) -> void {
    log_output_->appendPlainText(message);
}

auto MainWindow::current_output_directory() const -> std::filesystem::path {
    return std::filesystem::path(output_directory_edit_->text().toStdWString());
}

auto MainWindow::resolve_encoder_summary() const -> QString {
    const auto resolved = resolve_encoder(settings_, environment_);
    auto description = QString::fromStdString(resolved.video_codec);

    if (settings_.encoder_kind == EncoderKind::Auto) {
        description = QStringLiteral("Auto -> %1").arg(description);
    }

    if (environment_.has_nvidia_gpu) {
        description += " | NVIDIA GPU detected";
    } else {
        description += " | No NVIDIA GPU detected";
    }

    if (environment_.has_hevc_nvenc_encoder) {
        description += " | ffmpeg NVENC available";
    } else {
        description += " | ffmpeg NVENC unavailable";
    }

    return description;
}

} // namespace vidchopper
