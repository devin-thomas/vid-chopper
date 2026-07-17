#pragma once

#include "core/models.hpp"

#include <QAbstractTableModel>
#include <QModelIndexList>

#include <vector>

namespace vidchopper {

class ChapterTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ChapterTableModel(QObject* parent = nullptr);

    [[nodiscard]] auto rowCount(const QModelIndex& parent = QModelIndex {}) const -> int override;
    [[nodiscard]] auto columnCount(const QModelIndex& parent = QModelIndex {}) const -> int override;
    [[nodiscard]] auto data(const QModelIndex& index, int role = Qt::DisplayRole) const -> QVariant override;
    [[nodiscard]] auto headerData(
        int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const -> QVariant override;
    [[nodiscard]] auto flags(const QModelIndex& index) const -> Qt::ItemFlags override;
    auto setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) -> bool override;

    auto set_chapters(std::vector<ChapterSegment> chapters) -> void;
    [[nodiscard]] auto chapters() const -> const std::vector<ChapterSegment>&;
    [[nodiscard]] auto chapter_count() const -> int;
    [[nodiscard]] auto append_row_index() const -> int;
    [[nodiscard]] auto is_append_row(const QModelIndex& index) const -> bool;
    auto set_display_mode(TimestampDisplayMode mode) -> void;
    auto set_frame_rate(FrameRate frame_rate) -> void;
    auto append_chapter(u64 duration_ms) -> bool;
    auto remove_rows(const QModelIndexList& indices) -> void;

signals:
    void chapters_changed();

private:
    [[nodiscard]] auto chapter_at(int row) const -> const ChapterSegment*;
    [[nodiscard]] auto format_time(u64 milliseconds) const -> QString;
    [[nodiscard]] auto parse_time(const QString& value) const -> std::optional<u64>;

    std::vector<ChapterSegment> chapters_;
    TimestampDisplayMode display_mode_ {TimestampDisplayMode::Milliseconds};
    FrameRate frame_rate_ {};
};

} // namespace vidchopper
