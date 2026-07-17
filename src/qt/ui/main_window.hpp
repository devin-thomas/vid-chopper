#pragma once

#include "core/models.hpp"
#include "qt/demo_launch_options.hpp"
#include "qt/logging.hpp"

#include <QMainWindow>
#include <QPointer>

#include <vector>
#include <optional>

class QCheckBox;
class QCloseEvent;
class QEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSize;
class QSettings;
class QSpinBox;
class QTableView;
class QToolButton;

namespace vidchopper {

class ChapterTableModel;
class ExportCoordinator;
class AdvancedSettingsDialog;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(DemoLaunchOptions demo_options = {}, QWidget* parent = nullptr);

protected:
    auto closeEvent(QCloseEvent* event) -> void override;
    auto eventFilter(QObject* watched, QEvent* event) -> bool override;

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
    auto load_video(const QString& source_path) -> bool;
    auto apply_settings_to_ui() -> void;
    auto apply_zoom_percent(int zoom_percent, bool persist) -> void;
    auto refresh_summary() -> void;
    auto update_chapter_table_columns() -> void;
    auto update_export_button_style() -> void;
    auto update_log_disclosure(bool expanded) -> void;
    auto refresh_log_view() -> void;
    auto append_log_message(LogCategory category, const QString& message) -> void;
    auto set_output_directory_path(const std::filesystem::path& path, bool overridden) -> void;
    auto activate_demo_scene() -> void;
    auto seed_workspace_demo(bool show_logs) -> bool;
    auto seed_settings_precision_demo() -> bool;
    auto select_demo_chapter_row(int row) -> void;
    auto write_demo_ready_file(const QString& status) const -> void;
    [[nodiscard]] auto confirm_exit() -> bool;
    [[nodiscard]] auto current_screen_size() const -> QSize;
    [[nodiscard]] auto current_output_directory() const -> std::filesystem::path;
    [[nodiscard]] auto resolve_encoder_summary() const -> QString;

    std::optional<VideoMetadata> metadata_;
    QString config_path_;
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
    QToolButton* log_toggle_button_ {nullptr};
    QWidget* log_panel_ {nullptr};
    QCheckBox* advanced_logs_checkbox_ {nullptr};
    QPlainTextEdit* log_output_ {nullptr};
    QProgressBar* progress_bar_ {nullptr};
    QPushButton* export_button_ {nullptr};

    std::filesystem::path output_directory_path_;
    std::vector<LogEntry> log_entries_;
    int base_font_point_size_ {10};
    int zoom_percent_ {100};
    bool output_directory_overridden_ {false};
    bool demo_scene_applied_ {false};
    DemoLaunchOptions demo_options_;
    QPointer<AdvancedSettingsDialog> demo_settings_dialog_;
};

} // namespace vidchopper
