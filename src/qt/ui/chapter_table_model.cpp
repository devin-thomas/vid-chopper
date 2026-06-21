#include "qt/ui/chapter_table_model.h"

#include "core/string_utils.h"
#include "core/timecode.h"

#include <QBrush>
#include <QSet>

#include <algorithm>

namespace vidchopper {

namespace {

enum Column : int {
    Name = 0,
    Start = 1,
    End = 2,
    Duration = 3,
    Count = 4,
};

} // namespace

ChapterTableModel::ChapterTableModel(QObject* parent)
    : QAbstractTableModel(parent) {
}

auto ChapterTableModel::rowCount(const QModelIndex& parent) const -> int {
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(chapters_.size());
}

auto ChapterTableModel::columnCount(const QModelIndex& parent) const -> int {
    if (parent.isValid()) {
        return 0;
    }

    return Column::Count;
}

auto ChapterTableModel::data(const QModelIndex& index, const int role) const -> QVariant {
    if (!index.isValid() || index.row() >= rowCount()) {
        return {};
    }

    const auto& chapter = chapters_[static_cast<usize>(index.row())];
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case Column::Name:
            return QString::fromStdString(chapter.name);
        case Column::Start:
            return format_time(chapter.start_ms);
        case Column::End:
            return format_time(chapter.end_ms);
        case Column::Duration:
            return format_time(chapter.end_ms - chapter.start_ms);
        default:
            break;
        }
    }

    if (role == Qt::ForegroundRole && chapter.end_ms <= chapter.start_ms) {
        return QBrush {QColor(255, 120, 120)};
    }

    return {};
}

auto ChapterTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const -> QVariant {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch (section) {
    case Column::Name:
        return "Chapter";
    case Column::Start:
        return display_mode_ == TimestampDisplayMode::Frames ? "Start (HH:MM:SS:FF)" : "Start (HH:MM:SS.mmm)";
    case Column::End:
        return display_mode_ == TimestampDisplayMode::Frames ? "End (HH:MM:SS:FF)" : "End (HH:MM:SS.mmm)";
    case Column::Duration:
        return "Length";
    default:
        return {};
    }
}

auto ChapterTableModel::flags(const QModelIndex& index) const -> Qt::ItemFlags {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() != Column::Duration) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

auto ChapterTableModel::setData(const QModelIndex& index, const QVariant& value, const int role) -> bool {
    if (!index.isValid() || role != Qt::EditRole || index.row() >= rowCount()) {
        return false;
    }

    auto& chapter = chapters_[static_cast<usize>(index.row())];
    switch (index.column()) {
    case Column::Name:
        chapter.name = trim_copy(value.toString().toStdString());
        break;
    case Column::Start: {
        const auto parsed = parse_time(value.toString());
        if (!parsed.has_value()) {
            return false;
        }
        chapter.start_ms = *parsed;
        break;
    }
    case Column::End: {
        const auto parsed = parse_time(value.toString());
        if (!parsed.has_value()) {
            return false;
        }
        chapter.end_ms = *parsed;
        break;
    }
    default:
        return false;
    }

    emit dataChanged(this->index(index.row(), 0), this->index(index.row(), Column::Duration));
    emit chapters_changed();
    return true;
}

auto ChapterTableModel::set_chapters(std::vector<ChapterSegment> chapters) -> void {
    beginResetModel();
    chapters_ = std::move(chapters);
    endResetModel();
    emit chapters_changed();
}

auto ChapterTableModel::chapters() const -> const std::vector<ChapterSegment>& {
    return chapters_;
}

auto ChapterTableModel::set_display_mode(const TimestampDisplayMode mode) -> void {
    display_mode_ = mode;
    emit headerDataChanged(Qt::Horizontal, Column::Start, Column::End);
    if (rowCount() > 0) {
        emit dataChanged(index(0, Column::Start), index(rowCount() - 1, Column::Duration));
    }
}

auto ChapterTableModel::set_frame_rate(const FrameRate frame_rate) -> void {
    frame_rate_ = frame_rate;
    if (rowCount() > 0) {
        emit dataChanged(index(0, Column::Start), index(rowCount() - 1, Column::Duration));
    }
}

auto ChapterTableModel::append_chapter(const u64 duration_ms) -> bool {
    if (duration_ms < 1000) {
        return false;
    }

    if (chapters_.empty()) {
        beginInsertRows(QModelIndex {}, 0, 0);
        chapters_.push_back(ChapterSegment {
            .name = "Chapter 1",
            .start_ms = 0,
            .end_ms = duration_ms,
        });
        endInsertRows();
        emit chapters_changed();
        return true;
    }

    auto& last = chapters_.back();
    const auto last_duration = last.end_ms - last.start_ms;
    if (last_duration < 2000) {
        return false;
    }

    const auto split_point = last.start_ms + (last_duration / 2);
    last.end_ms = split_point;

    beginInsertRows(QModelIndex {}, rowCount(), rowCount());
    chapters_.push_back(ChapterSegment {
        .name = "Chapter " + std::to_string(chapters_.size() + 1),
        .start_ms = split_point,
        .end_ms = duration_ms,
    });
    endInsertRows();
    emit dataChanged(index(rowCount() - 2, Column::Start), index(rowCount() - 2, Column::Duration));
    emit chapters_changed();
    return true;
}

auto ChapterTableModel::remove_rows(const QModelIndexList& indices) -> void {
    auto rows = QSet<int> {};
    for (const auto& index : indices) {
        rows.insert(index.row());
    }

    auto sorted_rows = rows.values();
    std::sort(sorted_rows.begin(), sorted_rows.end(), std::greater {});

    for (const auto row : sorted_rows) {
        if (row < 0 || row >= rowCount()) {
            continue;
        }

        beginRemoveRows(QModelIndex {}, row, row);
        chapters_.erase(chapters_.begin() + row);
        endRemoveRows();
    }

    emit chapters_changed();
}

auto ChapterTableModel::format_time(const u64 milliseconds) const -> QString {
    return display_mode_ == TimestampDisplayMode::Frames
        ? QString::fromStdString(format_frame_timecode(milliseconds, frame_rate_))
        : QString::fromStdString(format_millisecond_timecode(milliseconds));
}

auto ChapterTableModel::parse_time(const QString& value) const -> std::optional<u64> {
    return display_mode_ == TimestampDisplayMode::Frames
        ? parse_frame_timecode(value.toStdString(), frame_rate_)
        : parse_millisecond_timecode(value.toStdString());
}

} // namespace vidchopper
