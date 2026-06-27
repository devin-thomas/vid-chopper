#pragma once

#include "core/models.h"

#include <QMainWindow>

#include <optional>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSettings;
class QSpinBox;
class QTableView;

namespace vidchopper {

class ChapterTableModel;
class ExportCoordinator;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void open_video();
    void choose_output_directory();
    void reset_output_directory();
    void import_embedded_chapters();
    void redistribute_chapters();
    void add_chapter();
    void remove_selected_chapters();
    void open_advanced_settings();
    void redetect_gpu();
    void start_or_cancel_export();
    void handle_export_finished(bool success, const QStringList& errors);

private:
    auto create_menus() -> void;
    auto build_ui() -> void;
    auto load_video(const QString& source_path) -> void;
    auto apply_settings_to_ui() -> void;
    auto refresh_summary() -> void;
    auto append_log_message(const QString& message) -> void;
    auto set_export_button_state(bool exporting) -> void;
    [[nodiscard]] auto current_output_directory() const -> std::filesystem::path;
    [[nodiscard]] auto resolve_encoder_summary() const -> QString;

    std::optional<VideoMetadata> metadata_;
    QSettings* settings_store_ {nullptr};
    ExportSettings settings_;
    EncoderEnvironment environment_;

    ChapterTableModel* chapter_model_ {nullptr};
    ExportCoordinator* export_coordinator_ {nullptr};

    QLineEdit* source_path_edit_ {nullptr};
    QLineEdit* output_directory_edit_ {nullptr};
    QLabel* duration_value_label_ {nullptr};
    QLabel* frame_rate_value_label_ {nullptr};
    QLabel* chapter_source_value_label_ {nullptr};
    QLabel* encoder_value_label_ {nullptr};
    QSpinBox* chapter_count_spin_ {nullptr};
    QTableView* chapter_table_ {nullptr};
    QPlainTextEdit* log_output_ {nullptr};
    QProgressBar* progress_bar_ {nullptr};
    QPushButton* export_button_ {nullptr};

    bool output_directory_overridden_ {false};
};

} // namespace vidchopper
