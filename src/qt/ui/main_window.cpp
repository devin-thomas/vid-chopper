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
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
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
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QScrollBar>
#include <QStatusBar>
#include <QTableView>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <array>
#include <algorithm>
#include <filesystem>

namespace vidchopper {

namespace {

#ifndef VIDCHOPPER_DISPLAY_VERSION
#define VIDCHOPPER_DISPLAY_VERSION "0.2.0-alpha"
#endif

constexpr auto demo_chapter_names = std::array {
    "Intro",
    "Setup Overview",
    "Key Features",
    "Demo",
    "Tips & Tricks",
    "Outro",
};

auto normalize_path_for_storage(const std::filesystem::path& path) -> std::filesystem::path {
    auto error = std::error_code {};
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }

    return std::filesystem::absolute(path, error).lexically_normal();
}

auto display_path(const std::filesystem::path& path) -> QString {
    return QDir::toNativeSeparators(QString::fromStdWString(path.wstring()));
}

auto translated_log_message(const LogEntry& entry) -> QString {
    if (entry.category == LogCategory::ProcessRaw) {
        return {};
    }

    if (entry.message.startsWith("Config file: ")) {
        return entry.message;
    }

    if (entry.message.startsWith("Loaded ")) {
        return QStringLiteral("Loaded video: %1").arg(entry.message.mid(QStringLiteral("Loaded ").size()));
    }

    if (entry.message.startsWith("Writing ")) {
        return QStringLiteral("Preparing clip file: %1").arg(entry.message.mid(QStringLiteral("Writing ").size()));
    }

    if (entry.message.startsWith("Exporting ")) {
        return QStringLiteral("Running ffmpeg for: %1").arg(entry.message.mid(QStringLiteral("Exporting ").size()));
    }

    if (entry.message == "Starting export.") {
        return "Starting export.";
    }

    return entry.message;
}

auto demo_window_title() -> QString {
    const auto version = QStringLiteral(VIDCHOPPER_DISPLAY_VERSION);
    const auto lowered = version.toLower();
    const auto is_prerelease =
        lowered.contains("alpha") || lowered.contains("beta") || lowered.contains("nightly");
    return is_prerelease ? QStringLiteral("VidChopper %1").arg(version) : QStringLiteral("VidChopper");
}

auto build_seeded_demo_chapters(const u64 duration_ms) -> std::vector<ChapterSegment> {
    auto chapters = build_default_chapters(duration_ms, static_cast<u8>(demo_chapter_names.size()));
    for (auto index = usize {0}; index < chapters.size() && index < demo_chapter_names.size(); ++index) {
        chapters[index].name = demo_chapter_names[index];
    }
    return chapters;
}

} // namespace

MainWindow::MainWindow(DemoLaunchOptions demo_options, QWidget* parent)
    : QMainWindow(parent)
    , demo_options_(std::move(demo_options))
    , chapter_model_(new ChapterTableModel {this})
    , export_coordinator_(new ExportCoordinator {this}) {
    setWindowTitle(demo_window_title());
    resize(1280, 860);

    if (demo_options_.window_size.has_value()) {
        resize(demo_options_.window_size->width, demo_options_.window_size->height);
    }

    const auto settings_store = create_settings_store(this);
    settings_store_ = settings_store.settings;
    config_path_ = settings_store.config_path;
    settings_ = load_export_settings(*settings_store_);

    const auto stored_screen_size = load_last_screen_size(*settings_store_);
    const auto screen_size = current_screen_size();
    if (stored_screen_size != screen_size) {
        zoom_percent_ = auto_zoom_percent_for_screen_height(screen_size.height());
        save_zoom_percent(*settings_store_, zoom_percent_);
        save_last_screen_size(*settings_store_, screen_size);
        settings_store_->sync();
    } else {
        zoom_percent_ = load_zoom_percent(*settings_store_);
    }

    base_font_point_size_ =
        std::max(10, static_cast<int>(qApp->font().pointSizeF() > 0.0 ? qApp->font().pointSizeF() : 10.0));

    build_ui();
    create_menus();
    apply_settings_to_ui();
    apply_zoom_percent(zoom_percent_, false);
    redetect_gpu();

    qApp->installEventFilter(this);

    connect(chapter_model_, &ChapterTableModel::chapters_changed, this, [this]() {
        chapter_count_spin_->blockSignals(true);
        chapter_count_spin_->setValue(chapter_model_->chapter_count());
        chapter_count_spin_->blockSignals(false);
    });

    connect(chapter_table_, &QTableView::clicked, this, [this](const QModelIndex& index) {
        if (chapter_model_->is_append_row(index)) {
            add_chapter();
        }
    });

    connect(export_coordinator_, &ExportCoordinator::log_message, this, &MainWindow::append_log_message);
    connect(export_coordinator_, &ExportCoordinator::progress_changed, progress_bar_, &QProgressBar::setValue);
    connect(export_coordinator_,
        &ExportCoordinator::chapter_started,
        this,
        [this](const int current, const int total, const QString& output_file) {
            statusBar()->showMessage(QStringLiteral("Exporting chapter %1 of %2").arg(current).arg(total));
            append_log_message(LogCategory::ExportProgress, QStringLiteral("Writing %1").arg(output_file));
        });
    connect(export_coordinator_, &ExportCoordinator::finished, this, &MainWindow::handle_export_finished);

    append_log_message(LogCategory::Config, QStringLiteral("Config file: %1").arg(config_path_));

    if (demo_options_.enabled()) {
        QTimer::singleShot(0, this, &MainWindow::activate_demo_scene);
    }
}

auto MainWindow::closeEvent(QCloseEvent* event) -> void {
    if (!confirm_exit()) {
        event->ignore();
        return;
    }

    QMainWindow::closeEvent(event);
}

auto MainWindow::eventFilter(QObject* watched, QEvent* event) -> bool {
    if (event->type() == QEvent::Wheel) {
        auto* wheel_event = static_cast<QWheelEvent*>(event);
        if (wheel_event->modifiers().testFlag(Qt::ControlModifier)) {
            apply_zoom_percent(zoom_percent_ + (wheel_event->angleDelta().y() > 0 ? 25 : -25), true);
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

auto MainWindow::create_menus() -> void {
    auto* file_menu = menuBar()->addMenu("&File");
    file_menu->addAction("&Open Video...", this, &MainWindow::open_video, QKeySequence::Open);
    file_menu->addAction("Choose &Output Directory...", this, &MainWindow::choose_output_directory);
    file_menu->addSeparator();
    file_menu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);

    auto* view_menu = menuBar()->addMenu("&View");
    auto* zoom_in_action = new QAction {"Zoom &In", this};
    zoom_in_action->setShortcuts({QKeySequence {"Ctrl+="}, QKeySequence {"Ctrl++"}});
    connect(zoom_in_action, &QAction::triggered, this, [this]() { apply_zoom_percent(zoom_percent_ + 25, true); });
    view_menu->addAction(zoom_in_action);

    auto* zoom_out_action = new QAction {"Zoom &Out", this};
    zoom_out_action->setShortcut(QKeySequence {"Ctrl+-"});
    connect(zoom_out_action, &QAction::triggered, this, [this]() { apply_zoom_percent(zoom_percent_ - 25, true); });
    view_menu->addAction(zoom_out_action);

    auto* reset_zoom_action = new QAction {"&Reset Zoom", this};
    connect(reset_zoom_action, &QAction::triggered, this, [this]() {
        apply_zoom_percent(auto_zoom_percent_for_screen_height(current_screen_size().height()), true);
    });
    view_menu->addAction(reset_zoom_action);
    view_menu->addSeparator();

    auto* preset_menu = view_menu->addMenu("Zoom &Presets");
    for (auto zoom_percent = 50; zoom_percent <= 300; zoom_percent += 25) {
        auto* preset_action = new QAction {QStringLiteral("%1%").arg(zoom_percent), this};
        connect(preset_action, &QAction::triggered, this, [this, zoom_percent]() {
            apply_zoom_percent(zoom_percent, true);
        });
        preset_menu->addAction(preset_action);
    }

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
        QMessageBox::about(this,
            "About VidChopper",
            "VidChopper is a Windows-first Qt desktop application for turning one source video into chapter clips with ffmpeg.");
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

    auto* chapter_controls = new QWidget {central};
    auto* chapter_controls_layout = new QHBoxLayout {chapter_controls};
    chapter_controls_layout->setContentsMargins(0, 0, 0, 0);
    auto* add_button = new QPushButton {"Add Chapter", chapter_controls};
    connect(add_button, &QPushButton::clicked, this, &MainWindow::add_chapter);
    chapter_controls_layout->addWidget(add_button);
    chapter_controls_layout->addStretch(1);

    chapter_table_ = new QTableView {central};
    chapter_table_->setModel(chapter_model_);
    chapter_table_->horizontalHeader()->setStretchLastSection(false);
    chapter_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    chapter_table_->verticalHeader()->setVisible(false);
    chapter_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    chapter_table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    chapter_table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chapter_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto* export_row = new QWidget {central};
    auto* export_layout = new QHBoxLayout {export_row};
    progress_bar_ = new QProgressBar {export_row};
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    export_button_ = new QPushButton {"Export Chapters", export_row};
    connect(export_button_, &QPushButton::clicked, this, &MainWindow::start_or_cancel_export);
    export_layout->addWidget(progress_bar_, 1);
    export_layout->addWidget(export_button_);

    log_toggle_button_ = new QToolButton {central};
    log_toggle_button_->setCheckable(true);
    log_toggle_button_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(log_toggle_button_, &QToolButton::toggled, this, &MainWindow::update_log_disclosure);

    log_panel_ = new QWidget {central};
    auto* log_layout = new QVBoxLayout {log_panel_};
    log_layout->setContentsMargins(0, 0, 0, 0);
    advanced_logs_checkbox_ = new QCheckBox {"Advanced", log_panel_};
    connect(advanced_logs_checkbox_, &QCheckBox::toggled, this, [this]() { refresh_log_view(); });
    log_output_ = new QPlainTextEdit {log_panel_};
    log_output_->setReadOnly(true);
    log_output_->setPlaceholderText("Curated export activity appears here when logs are expanded.");
    log_layout->addWidget(advanced_logs_checkbox_, 0, Qt::AlignLeft);
    log_layout->addWidget(log_output_, 1);

    root_layout->addWidget(source_group);
    root_layout->addWidget(summary_group);
    root_layout->addWidget(chapter_controls);
    root_layout->addWidget(chapter_table_, 1);
    root_layout->addWidget(export_row);
    root_layout->addWidget(log_toggle_button_);
    root_layout->addWidget(log_panel_, 1);

    setCentralWidget(central);
    statusBar()->showMessage("Ready");
    update_log_disclosure(false);
    update_chapter_table_columns();
    update_export_button_style();
}

auto MainWindow::open_video() -> void {
    const auto file_path = QFileDialog::getOpenFileName(this,
        "Select a source video",
        QString {},
        "Video Files (*.mp4 *.mkv *.mov *.m4v *.avi *.webm);;All Files (*.*)");

    if (!file_path.isEmpty()) {
        static_cast<void>(load_video(file_path));
    }
}

auto MainWindow::choose_output_directory() -> void {
    const auto directory =
        QFileDialog::getExistingDirectory(this, "Choose output directory", output_directory_edit_->text());
    if (!directory.isEmpty()) {
        set_output_directory_path(normalize_path_for_storage(std::filesystem::path {directory.toStdWString()}), true);
    }
}

auto MainWindow::reset_output_directory() -> void {
    if (!metadata_.has_value()) {
        return;
    }

    set_output_directory_path(default_output_directory(metadata_->source_path, settings_), false);
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

    chapter_model_->set_chapters(
        build_default_chapters(metadata_->duration_ms, static_cast<u8>(chapter_count_spin_->value())));
    chapter_source_value_label_->setText("Evenly distributed from the current chapter count");
}

auto MainWindow::add_chapter() -> void {
    if (!metadata_.has_value()) {
        return;
    }

    if (!chapter_model_->append_chapter(metadata_->duration_ms)) {
        QMessageBox::warning(this,
            "Cannot add chapter",
            "VidChopper could not split the current layout into another chapter while keeping every segment at least one second long.");
    }
}

auto MainWindow::remove_selected_chapters() -> void {
    const auto selected_rows = chapter_table_->selectionModel()->selectedRows();
    auto removable_rows = QModelIndexList {};
    for (const auto& index : selected_rows) {
        if (!chapter_model_->is_append_row(index)) {
            removable_rows.push_back(index);
        }
    }

    if (removable_rows.isEmpty()) {
        return;
    }

    if (settings_.confirm_remove_chapters) {
        const auto reply = QMessageBox::warning(this,
            "Remove chapters",
            QStringLiteral("Remove %1 selected chapter%2?")
                .arg(removable_rows.size())
                .arg(removable_rows.size() == 1 ? "" : "s"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    chapter_model_->remove_rows(removable_rows);
}

auto MainWindow::open_advanced_settings() -> void {
    auto dialog = AdvancedSettingsDialog {this};
    dialog.set_settings(settings_);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    settings_ = dialog.settings();
    save_export_settings(*settings_store_, settings_);
    settings_store_->sync();
    apply_settings_to_ui();
    if (!output_directory_overridden_ && metadata_.has_value()) {
        reset_output_directory();
    }
    refresh_summary();
}

auto MainWindow::redetect_gpu() -> void {
    environment_ = GpuDetector::detect(QString::fromStdString(settings_.ffmpeg_path));
    refresh_summary();
    append_log_message(LogCategory::App, resolve_encoder_summary());
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

    append_log_message(LogCategory::ExportLifecycle, "Starting export.");
    progress_bar_->setValue(0);
    export_coordinator_->start_export(*metadata_, chapters, current_output_directory(), settings_, environment_);
    update_export_button_style();
}

auto MainWindow::handle_export_finished(const bool success, const QStringList& errors) -> void {
    statusBar()->showMessage(success ? "Export complete" : "Export ended with errors");
    update_export_button_style();

    for (const auto& error : errors) {
        append_log_message(LogCategory::Error, error);
    }

    if (success) {
        append_log_message(LogCategory::ExportLifecycle, "Export complete.");
    }

    if (success && settings_.open_output_directory_after_export) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(output_directory_edit_->text()));
    }
}

auto MainWindow::load_video(const QString& source_path) -> bool {
    const auto probe_result = FfprobeService::probe_video(QString::fromStdString(settings_.ffprobe_path), source_path);
    if (!probe_result.success) {
        QMessageBox::critical(this, "ffprobe error", probe_result.error_message);
        append_log_message(LogCategory::Error, probe_result.error_message);
        return false;
    }

    metadata_ = probe_result.metadata;
    source_path_edit_->setText(display_path(metadata_->source_path));

    if (settings_.prefer_embedded_chapters && !metadata_->embedded_chapters.empty()) {
        chapter_model_->set_chapters(metadata_->embedded_chapters);
        chapter_source_value_label_->setText("Embedded chapter metadata");
    } else {
        chapter_model_->set_chapters(build_default_chapters(metadata_->duration_ms, settings_.default_chapter_count));
        chapter_source_value_label_->setText("Evenly distributed starter layout");
    }

    chapter_count_spin_->setValue(chapter_model_->chapter_count());
    chapter_model_->set_frame_rate(metadata_->frame_rate);

    if (!output_directory_overridden_) {
        set_output_directory_path(default_output_directory(metadata_->source_path, settings_), false);
    }

    refresh_summary();
    append_log_message(LogCategory::Probe, QStringLiteral("Loaded %1").arg(display_path(metadata_->source_path)));
    return true;
}

auto MainWindow::apply_settings_to_ui() -> void {
    chapter_count_spin_->setValue(settings_.default_chapter_count);
    chapter_model_->set_display_mode(settings_.display_mode);
    update_chapter_table_columns();
}

auto MainWindow::apply_zoom_percent(const int zoom_percent, const bool persist) -> void {
    zoom_percent_ = clamp_zoom_percent(zoom_percent);

    auto font = qApp->font();
    font.setPointSizeF(static_cast<double>(base_font_point_size_) * static_cast<double>(zoom_percent_) / 100.0);
    qApp->setFont(font);

    const auto control_padding = std::max(6, zoom_percent_ / 20);
    const auto row_height = std::max(28, zoom_percent_ / 4);
    const auto section_padding = std::max(4, zoom_percent_ / 30);
    qApp->setStyleSheet(
        QStringLiteral("QPushButton, QToolButton { padding:%1px %2px; }"
                       "QLineEdit, QComboBox, QSpinBox { min-height:%3px; }"
                       "QHeaderView::section { padding:%4px; }"
                       "QProgressBar { min-height:%3px; }"
                       "QPlainTextEdit { font-family:'Cascadia Mono','Consolas','Courier New',monospace; }")
            .arg(control_padding)
            .arg(control_padding * 2)
            .arg(row_height)
            .arg(section_padding));

    update_chapter_table_columns();
    update_export_button_style();

    if (persist) {
        save_zoom_percent(*settings_store_, zoom_percent_);
        save_last_screen_size(*settings_store_, current_screen_size());
        settings_store_->sync();
    }
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

auto MainWindow::update_chapter_table_columns() -> void {
    auto widest_header = 0;
    const auto metrics = QFontMetrics {chapter_table_->horizontalHeader()->font()};
    for (auto column = 0; column < chapter_model_->columnCount(); ++column) {
        widest_header = std::max(widest_header,
            metrics.horizontalAdvance(chapter_model_->headerData(column, Qt::Horizontal).toString()) + 28);
    }

    chapter_table_->horizontalHeader()->setMinimumSectionSize(widest_header);
    chapter_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

auto MainWindow::update_export_button_style() -> void {
    const auto exporting = export_coordinator_->busy();
    const auto background = exporting ? QStringLiteral("#c75050") : QStringLiteral("#2f7fe7");
    const auto border = exporting ? QStringLiteral("#e36c6c") : QStringLiteral("#5ca0ff");
    const auto hover = exporting ? QStringLiteral("#da5f5f") : QStringLiteral("#4c91f2");
    const auto radius = std::max(8, zoom_percent_ / 15);
    const auto vertical_padding = std::max(8, zoom_percent_ / 18);
    const auto horizontal_padding = std::max(18, zoom_percent_ / 8);
    export_button_->setText(exporting ? "Cancel Export" : "Export Chapters");
    export_button_->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:white; border:1px solid %2; border-radius:%3px; font-weight:700; padding:%4px %5px; }"
        "QPushButton:hover { background:%6; }")
                                      .arg(background)
                                      .arg(border)
                                      .arg(radius)
                                      .arg(vertical_padding)
                                      .arg(horizontal_padding)
                                      .arg(hover));
}

auto MainWindow::update_log_disclosure(const bool expanded) -> void {
    log_toggle_button_->blockSignals(true);
    log_toggle_button_->setChecked(expanded);
    log_toggle_button_->blockSignals(false);
    log_toggle_button_->setText(expanded ? "▾ Hide Logs" : "▸ Show Logs");
    log_panel_->setVisible(expanded);
}

auto MainWindow::refresh_log_view() -> void {
    auto lines = QStringList {};
    for (const auto& entry : log_entries_) {
        const auto translated = advanced_logs_checkbox_->isChecked() ? entry.message : translated_log_message(entry);
        if (!translated.isEmpty()) {
            lines.push_back(translated);
        }
    }

    log_output_->setPlainText(lines.join('\n'));
    auto* scrollbar = log_output_->verticalScrollBar();
    if (scrollbar != nullptr) {
        scrollbar->setValue(scrollbar->maximum());
    }
}

auto MainWindow::append_log_message(const LogCategory category, const QString& message) -> void {
    log_entries_.push_back(LogEntry {
        .category = category,
        .message = message,
    });
    refresh_log_view();
}

auto MainWindow::set_output_directory_path(const std::filesystem::path& path, const bool overridden) -> void {
    output_directory_path_ = normalize_path_for_storage(path);
    output_directory_edit_->setText(display_path(output_directory_path_));
    output_directory_overridden_ = overridden;
}

auto MainWindow::activate_demo_scene() -> void {
    if (demo_scene_applied_ || !demo_options_.enabled()) {
        return;
    }

    demo_scene_applied_ = true;

    auto success = false;
    switch (demo_options_.scene) {
    case DemoScene::Workspace:
        success = seed_workspace_demo(false);
        break;
    case DemoScene::WorkspaceLogs:
        success = seed_workspace_demo(true);
        break;
    case DemoScene::SettingsPrecision:
        success = seed_settings_precision_demo();
        break;
    case DemoScene::None:
        success = true;
        break;
    }

    const auto status = success ? QStringLiteral("ready") : QStringLiteral("error");
    QTimer::singleShot(0, this, [this, status]() { write_demo_ready_file(status); });
}

auto MainWindow::seed_workspace_demo(const bool show_logs) -> bool {
    if (!load_video(QString::fromStdWString(demo_options_.demo_source.wstring()))) {
        return false;
    }

    chapter_model_->set_display_mode(TimestampDisplayMode::Milliseconds);
    chapter_model_->set_chapters(build_seeded_demo_chapters(metadata_->duration_ms));
    chapter_source_value_label_->setText("Seeded demo layout");
    chapter_count_spin_->setValue(chapter_model_->chapter_count());

    const auto output_directory = demo_options_.demo_source.parent_path() / "captures";
    std::filesystem::create_directories(output_directory);
    set_output_directory_path(output_directory, true);

    select_demo_chapter_row(3);
    append_log_message(LogCategory::App, "Seeded demo workspace prepared.");
    append_log_message(LogCategory::App, QStringLiteral("Output path: %1").arg(output_directory_edit_->text()));

    if (show_logs) {
        update_log_disclosure(true);
        append_log_message(LogCategory::ExportLifecycle, "Reviewing chapter plan before export.");
        append_log_message(LogCategory::ExportProgress, "Previewing output naming and destination.");
    } else {
        update_log_disclosure(false);
    }

    return true;
}

auto MainWindow::seed_settings_precision_demo() -> bool {
    if (!seed_workspace_demo(false)) {
        return false;
    }

    auto* dialog = new AdvancedSettingsDialog {this};
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    dialog->setModal(false);
    dialog->set_settings(settings_);
    dialog->set_active_page(AdvancedSettingsDialog::Page::Precision);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    demo_settings_dialog_ = dialog;
    return true;
}

auto MainWindow::select_demo_chapter_row(const int row) -> void {
    if (row < 0 || row >= chapter_model_->chapter_count()) {
        return;
    }

    chapter_table_->selectRow(row);
    chapter_table_->scrollTo(chapter_model_->index(row, 0), QAbstractItemView::PositionAtCenter);
}

auto MainWindow::write_demo_ready_file(const QString& status) const -> void {
    if (demo_options_.demo_ready_file.empty()) {
        return;
    }

    const auto ready_path = demo_options_.demo_ready_file;
    std::filesystem::create_directories(ready_path.parent_path());

    auto file = QFile {QString::fromStdWString(ready_path.wstring())};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }

    auto stream = QTextStream {&file};
    stream << status << '\n';
}

auto MainWindow::confirm_exit() -> bool {
    if (!settings_.confirm_exit) {
        return true;
    }

    const auto prompt = export_coordinator_->busy()
        ? QStringLiteral("An export is still running. Exit VidChopper anyway?")
        : QStringLiteral("Exit VidChopper?");
    const auto reply =
        QMessageBox::warning(this, "Confirm exit", prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return reply == QMessageBox::Yes;
}

auto MainWindow::current_screen_size() const -> QSize {
    const auto* active_screen = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    return active_screen == nullptr ? QSize {0, 0} : active_screen->geometry().size();
}

auto MainWindow::current_output_directory() const -> std::filesystem::path {
    return output_directory_path_;
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
