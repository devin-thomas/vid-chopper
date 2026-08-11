#pragma once

#include "core/models.hpp"
#include "qt/app_settings.hpp"
#include "qt/demo_launch_options.hpp"

#include <QMainWindow>
#include <QPointer>

#include <functional>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSettings;
class QSpinBox;
class QTableView;

namespace vidchopper {

class ChapterTableModel;
class AdvancedSettingsDialog;
class DemoAutomationController;
class ExportCoordinator;
class GpuDetector;
class LogViewController;
class PresentationController;
class SessionController;

class MainWindow final : public QMainWindow {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)

public:
    explicit MainWindow(DemoLaunchOptions demo_options = {}, QWidget* parent = nullptr);

protected:
    auto closeEvent(QCloseEvent* event) -> void override;

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
    [[nodiscard]] auto request_video_load(const Path& source_path, std::function<void(bool)> completion = {}) -> bool;
    auto handle_video_loaded() -> void;
    auto handle_video_load_failed(const QString& error_message) -> void;
    auto apply_settings_to_ui() -> void;
    auto refresh_summary() -> void;
    auto update_chapter_table_columns() -> void;
    auto update_export_button_style() -> void;
    [[nodiscard]] auto persist_app_settings() -> bool;
    auto set_output_directory_path(const std::filesystem::path& path, bool overridden) -> void;
    [[nodiscard]] auto seed_demo_scene(DemoScene scene) -> bool;
    auto seed_workspace_demo(bool show_logs) -> bool;
    auto seed_settings_precision_demo() -> bool;
    auto select_demo_chapter_row(int row) -> void;
    [[nodiscard]] auto confirm_exit() -> bool;
    [[nodiscard]] auto current_metadata() const -> const VideoMetadata*;
    [[nodiscard]] auto current_output_directory() const -> std::filesystem::path;
    [[nodiscard]] auto resolve_encoder_summary() const -> QString;

    QString config_path_;
    QString settings_store_error_;
    bool settings_store_available_ {true};
    QSettings* settings_store_ {nullptr};
    ExportSettings settings_;
    EncoderEnvironment environment_;

    ChapterTableModel* chapter_model_ {nullptr};
    SessionController* session_controller_ {nullptr};
    ExportCoordinator* export_coordinator_ {nullptr};
    GpuDetector* gpu_detector_ {nullptr};
    PresentationController* presentation_controller_ {nullptr};
    DemoAutomationController* demo_controller_ {nullptr};
    LogViewController* log_controller_ {nullptr};

    QLineEdit* source_path_edit_ {nullptr};
    QLineEdit* output_directory_edit_ {nullptr};
    QLabel* duration_value_label_ {nullptr};
    QLabel* frame_rate_value_label_ {nullptr};
    QLabel* chapter_source_value_label_ {nullptr};
    QLabel* encoder_value_label_ {nullptr};
    QSpinBox* chapter_count_spin_ {nullptr};
    QTableView* chapter_table_ {nullptr};
    QProgressBar* progress_bar_ {nullptr};
    QPushButton* export_button_ {nullptr};

    QPointer<AdvancedSettingsDialog> demo_settings_dialog_;
};

} // namespace vidchopper
