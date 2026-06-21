#pragma once

#include "core/models.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>

namespace vidchopper {

class AdvancedSettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AdvancedSettingsDialog(QWidget* parent = nullptr);

    auto set_settings(const ExportSettings& settings) -> void;
    [[nodiscard]] auto settings() const -> ExportSettings;

private:
    QComboBox* encoder_combo_ {nullptr};
    QComboBox* audio_combo_ {nullptr};
    QComboBox* container_combo_ {nullptr};
    QComboBox* overwrite_combo_ {nullptr};
    QComboBox* seek_combo_ {nullptr};
    QComboBox* display_combo_ {nullptr};

    QSpinBox* default_chapter_spin_ {nullptr};
    QSpinBox* index_padding_spin_ {nullptr};
    QSpinBox* x264_crf_spin_ {nullptr};
    QSpinBox* nvenc_cq_spin_ {nullptr};
    QSpinBox* min_chapter_spin_ {nullptr};
    QSpinBox* threads_spin_ {nullptr};
    QSpinBox* aac_bitrate_spin_ {nullptr};

    QLineEdit* output_folder_pattern_edit_ {nullptr};
    QLineEdit* naming_pattern_edit_ {nullptr};
    QLineEdit* x264_preset_edit_ {nullptr};
    QLineEdit* nvenc_preset_edit_ {nullptr};
    QLineEdit* ffmpeg_path_edit_ {nullptr};
    QLineEdit* ffprobe_path_edit_ {nullptr};
    QLineEdit* extra_args_edit_ {nullptr};

    QCheckBox* auto_gpu_checkbox_ {nullptr};
    QCheckBox* sanitize_names_checkbox_ {nullptr};
    QCheckBox* open_folder_checkbox_ {nullptr};
    QCheckBox* stop_on_error_checkbox_ {nullptr};
    QCheckBox* verify_durations_checkbox_ {nullptr};
    QCheckBox* write_json_manifest_checkbox_ {nullptr};
    QCheckBox* write_csv_manifest_checkbox_ {nullptr};
    QCheckBox* copy_metadata_checkbox_ {nullptr};
    QCheckBox* prefer_embedded_checkbox_ {nullptr};
};

} // namespace vidchopper
