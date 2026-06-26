#include "qt/ui/advanced_settings_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace vidchopper {

namespace {

template <typename Enum>
auto safe_enum_cast(int index, Enum max_valid, Enum fallback) -> Enum {
    if (index < 0 || index > static_cast<int>(max_valid)) {
        return fallback;
    }
    return static_cast<Enum>(index);
}

} // namespace

AdvancedSettingsDialog::AdvancedSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Advanced Settings");
    resize(680, 520);

    auto* tabs = new QTabWidget {this};

    auto* encoding_tab = new QWidget {tabs};
    auto* encoding_form = new QFormLayout {encoding_tab};
    encoder_combo_ = new QComboBox {encoding_tab};
    encoder_combo_->addItems({"Auto", "x264", "HEVC NVENC"});
    audio_combo_ = new QComboBox {encoding_tab};
    audio_combo_->addItems({"Copy source audio", "AAC re-encode"});
    x264_preset_edit_ = new QLineEdit {"slow", encoding_tab};
    nvenc_preset_edit_ = new QLineEdit {"p5", encoding_tab};
    x264_crf_spin_ = new QSpinBox {encoding_tab};
    x264_crf_spin_->setRange(0, 51);
    nvenc_cq_spin_ = new QSpinBox {encoding_tab};
    nvenc_cq_spin_->setRange(0, 51);
    aac_bitrate_spin_ = new QSpinBox {encoding_tab};
    aac_bitrate_spin_->setRange(64, 512);
    aac_bitrate_spin_->setSuffix(" kbps");
    threads_spin_ = new QSpinBox {encoding_tab};
    threads_spin_->setRange(0, 64);
    threads_spin_->setSpecialValueText("Auto");
    auto_gpu_checkbox_ = new QCheckBox {"Automatically switch to HEVC NVENC when NVIDIA hardware is detected", encoding_tab};

    encoding_form->addRow("Video encoder", encoder_combo_);
    encoding_form->addRow("Audio handling", audio_combo_);
    encoding_form->addRow("x264 preset", x264_preset_edit_);
    encoding_form->addRow("x264 CRF", x264_crf_spin_);
    encoding_form->addRow("NVENC preset", nvenc_preset_edit_);
    encoding_form->addRow("NVENC CQ", nvenc_cq_spin_);
    encoding_form->addRow("AAC bitrate", aac_bitrate_spin_);
    encoding_form->addRow("FFmpeg threads", threads_spin_);
    encoding_form->addRow("", auto_gpu_checkbox_);
    tabs->addTab(encoding_tab, "Encoding");

    auto* output_tab = new QWidget {tabs};
    auto* output_form = new QFormLayout {output_tab};
    container_combo_ = new QComboBox {output_tab};
    container_combo_->addItems({"Match source", "MP4", "MKV"});
    overwrite_combo_ = new QComboBox {output_tab};
    overwrite_combo_->addItems({"Ask before overwrite", "Overwrite existing files", "Skip existing files"});
    output_folder_pattern_edit_ = new QLineEdit {"%source%_chapters", output_tab};
    naming_pattern_edit_ = new QLineEdit {"%index% - %name%", output_tab};
    index_padding_spin_ = new QSpinBox {output_tab};
    index_padding_spin_->setRange(1, 6);
    sanitize_names_checkbox_ = new QCheckBox {"Sanitize output names for Windows-safe filenames", output_tab};
    open_folder_checkbox_ = new QCheckBox {"Open the output directory when the export finishes", output_tab};
    copy_metadata_checkbox_ = new QCheckBox {"Copy source metadata into each chapter clip", output_tab};

    output_form->addRow("Container mode", container_combo_);
    output_form->addRow("Overwrite policy", overwrite_combo_);
    output_form->addRow("Output folder pattern", output_folder_pattern_edit_);
    output_form->addRow("File naming pattern", naming_pattern_edit_);
    output_form->addRow("Index padding", index_padding_spin_);
    output_form->addRow("", sanitize_names_checkbox_);
    output_form->addRow("", open_folder_checkbox_);
    output_form->addRow("", copy_metadata_checkbox_);
    tabs->addTab(output_tab, "Output");

    auto* precision_tab = new QWidget {tabs};
    auto* precision_form = new QFormLayout {precision_tab};
    seek_combo_ = new QComboBox {precision_tab};
    seek_combo_->addItems({"Accurate trim", "Fast seek"});
    display_combo_ = new QComboBox {precision_tab};
    display_combo_->addItems({"Milliseconds", "Frames"});
    default_chapter_spin_ = new QSpinBox {precision_tab};
    default_chapter_spin_->setRange(1, 255);
    min_chapter_spin_ = new QSpinBox {precision_tab};
    min_chapter_spin_->setRange(1, 60);
    prefer_embedded_checkbox_ = new QCheckBox {"Use embedded chapters as the initial plan when they exist", precision_tab};
    stop_on_error_checkbox_ = new QCheckBox {"Stop immediately if a chapter export fails", precision_tab};
    verify_durations_checkbox_ = new QCheckBox {"Verify each finished chapter duration with ffprobe", precision_tab};
    write_json_manifest_checkbox_ = new QCheckBox {"Write a JSON export manifest", precision_tab};
    write_csv_manifest_checkbox_ = new QCheckBox {"Write a CSV export manifest", precision_tab};

    precision_form->addRow("Seek strategy", seek_combo_);
    precision_form->addRow("Timestamp editor mode", display_combo_);
    precision_form->addRow("Default chapter count", default_chapter_spin_);
    precision_form->addRow("Minimum chapter length", min_chapter_spin_);
    precision_form->addRow("", prefer_embedded_checkbox_);
    precision_form->addRow("", stop_on_error_checkbox_);
    precision_form->addRow("", verify_durations_checkbox_);
    precision_form->addRow("", write_json_manifest_checkbox_);
    precision_form->addRow("", write_csv_manifest_checkbox_);
    tabs->addTab(precision_tab, "Precision");

    auto* tools_tab = new QWidget {tabs};
    auto* tools_form = new QFormLayout {tools_tab};
    ffmpeg_path_edit_ = new QLineEdit {"ffmpeg", tools_tab};
    ffprobe_path_edit_ = new QLineEdit {"ffprobe", tools_tab};
    extra_args_edit_ = new QLineEdit {tools_tab};
    tools_form->addRow("ffmpeg executable", ffmpeg_path_edit_);
    tools_form->addRow("ffprobe executable", ffprobe_path_edit_);
    tools_form->addRow("Extra ffmpeg args", extra_args_edit_);
    tabs->addTab(tools_tab, "Tools");

    auto* buttons = new QDialogButtonBox {QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this};
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this]() {
        set_settings(ExportSettings {});
    });

    auto* layout = new QVBoxLayout {this};
    layout->addWidget(new QLabel {
        "Tokens: %source% uses the source filename stem, %index% uses the padded chapter number, and %name% uses the chapter title.",
        this,
    });
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

auto AdvancedSettingsDialog::set_settings(const ExportSettings& settings) -> void {
    encoder_combo_->setCurrentIndex(static_cast<int>(settings.encoder_kind));
    audio_combo_->setCurrentIndex(static_cast<int>(settings.audio_mode));
    container_combo_->setCurrentIndex(static_cast<int>(settings.container_mode));
    overwrite_combo_->setCurrentIndex(static_cast<int>(settings.overwrite_mode));
    seek_combo_->setCurrentIndex(static_cast<int>(settings.seek_mode));
    display_combo_->setCurrentIndex(static_cast<int>(settings.display_mode));

    default_chapter_spin_->setValue(settings.default_chapter_count);
    index_padding_spin_->setValue(settings.index_padding);
    x264_crf_spin_->setValue(settings.x264_crf);
    nvenc_cq_spin_->setValue(settings.nvenc_cq);
    min_chapter_spin_->setValue(settings.min_chapter_seconds);
    threads_spin_->setValue(settings.ffmpeg_threads);
    aac_bitrate_spin_->setValue(settings.aac_bitrate_kbps);

    output_folder_pattern_edit_->setText(QString::fromStdString(settings.output_folder_pattern));
    naming_pattern_edit_->setText(QString::fromStdString(settings.naming_pattern));
    x264_preset_edit_->setText(QString::fromStdString(settings.x264_preset));
    nvenc_preset_edit_->setText(QString::fromStdString(settings.nvenc_preset));
    ffmpeg_path_edit_->setText(QString::fromStdString(settings.ffmpeg_path));
    ffprobe_path_edit_->setText(QString::fromStdString(settings.ffprobe_path));
    extra_args_edit_->setText(QString::fromStdString(settings.extra_ffmpeg_args));

    auto_gpu_checkbox_->setChecked(settings.auto_detect_gpu);
    sanitize_names_checkbox_->setChecked(settings.sanitize_file_names);
    open_folder_checkbox_->setChecked(settings.open_output_directory_after_export);
    stop_on_error_checkbox_->setChecked(settings.stop_on_first_error);
    verify_durations_checkbox_->setChecked(settings.verify_output_durations);
    write_json_manifest_checkbox_->setChecked(settings.write_json_manifest);
    write_csv_manifest_checkbox_->setChecked(settings.write_csv_manifest);
    copy_metadata_checkbox_->setChecked(settings.copy_source_metadata);
    prefer_embedded_checkbox_->setChecked(settings.prefer_embedded_chapters);
}

auto AdvancedSettingsDialog::settings() const -> ExportSettings {
    auto values = ExportSettings {};

    values.encoder_kind = safe_enum_cast(encoder_combo_->currentIndex(), EncoderKind::HevcNvenc, EncoderKind::Auto);
    values.audio_mode = safe_enum_cast(audio_combo_->currentIndex(), AudioMode::Aac, AudioMode::Copy);
    values.container_mode = safe_enum_cast(container_combo_->currentIndex(), ContainerMode::Mkv, ContainerMode::Source);
    values.overwrite_mode = safe_enum_cast(overwrite_combo_->currentIndex(), OverwriteMode::Skip, OverwriteMode::Ask);
    values.seek_mode = safe_enum_cast(seek_combo_->currentIndex(), SeekMode::Fast, SeekMode::Accurate);
    values.display_mode = safe_enum_cast(display_combo_->currentIndex(), TimestampDisplayMode::Frames, TimestampDisplayMode::Milliseconds);

    values.default_chapter_count = static_cast<u8>(default_chapter_spin_->value());
    values.index_padding = static_cast<u8>(index_padding_spin_->value());
    values.x264_crf = static_cast<u8>(x264_crf_spin_->value());
    values.nvenc_cq = static_cast<u8>(nvenc_cq_spin_->value());
    values.min_chapter_seconds = static_cast<u8>(min_chapter_spin_->value());
    values.ffmpeg_threads = static_cast<u8>(threads_spin_->value());
    values.aac_bitrate_kbps = static_cast<u16>(aac_bitrate_spin_->value());

    values.output_folder_pattern = output_folder_pattern_edit_->text().toStdString();
    values.naming_pattern = naming_pattern_edit_->text().toStdString();
    values.x264_preset = x264_preset_edit_->text().toStdString();
    values.nvenc_preset = nvenc_preset_edit_->text().toStdString();
    values.ffmpeg_path = ffmpeg_path_edit_->text().toStdString();
    values.ffprobe_path = ffprobe_path_edit_->text().toStdString();
    values.extra_ffmpeg_args = extra_args_edit_->text().toStdString();

    values.auto_detect_gpu = auto_gpu_checkbox_->isChecked();
    values.sanitize_file_names = sanitize_names_checkbox_->isChecked();
    values.open_output_directory_after_export = open_folder_checkbox_->isChecked();
    values.stop_on_first_error = stop_on_error_checkbox_->isChecked();
    values.verify_output_durations = verify_durations_checkbox_->isChecked();
    values.write_json_manifest = write_json_manifest_checkbox_->isChecked();
    values.write_csv_manifest = write_csv_manifest_checkbox_->isChecked();
    values.copy_source_metadata = copy_metadata_checkbox_->isChecked();
    values.prefer_embedded_chapters = prefer_embedded_checkbox_->isChecked();

    return values;
}

} // namespace vidchopper
