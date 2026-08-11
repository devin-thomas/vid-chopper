#include "qt/ui/chapter_table_model.hpp"

#include "core/string_utils.hpp"
#include "core/timecode.hpp"
#include "qt/path_utils.hpp"

#include <QBrush>
#include <QSet>

#include <algorithm>
#include <cstddef>
#include <functional>

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

    return chapter_count() + 1;
}

auto ChapterTableModel::columnCount(const QModelIndex& parent) const -> int {
    if (parent.isValid()) {
        return 0;
    }

    return Column::Count;
}

auto ChapterTableModel::data(const QModelIndex& index, const int role) const -> QVariant {
    if (!index.isValid() || index.model() != this || index.row() < 0 || index.row() >= rowCount() || index.column() < 0
        || index.column() >= Column::Count) {
        return {};
    }

    if (is_append_row(index)) {
        if (role == Qt::DisplayRole && index.column() == Column::Name) {
            return QString::fromUtf8("➕ New Chapter…");
        }

        if (role == Qt::ForegroundRole) {
            return QBrush {QColor(120, 190, 255)};
        }

        return {};
    }

    const auto* chapter = chapter_at(index.row());
    if (chapter == nullptr) {
        return {};
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case Column::Name:
            return utf8_to_qstring(chapter->name);
        case Column::Start:
            return format_time(chapter->start_ms);
        case Column::End:
            return format_time(chapter->end_ms);
        case Column::Duration:
            return format_time(chapter->end_ms - chapter->start_ms);
        case Column::Count:
            return {};
        }
    }

    if (role == Qt::ForegroundRole && chapter->end_ms <= chapter->start_ms) {
        return QBrush {QColor(255, 120, 120)};
    }

    return {};
}

auto ChapterTableModel::headerData(
    const int section, const Qt::Orientation orientation, const int role) const -> QVariant {
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
    case Column::Count:
        return {};
    }
    return {};
}

auto ChapterTableModel::flags(const QModelIndex& index) const -> Qt::ItemFlags {
    if (!index.isValid() || index.model() != this || index.row() < 0 || index.row() >= rowCount() || index.column() < 0
        || index.column() >= Column::Count) {
        return Qt::NoItemFlags;
    }

    auto flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (!is_append_row(index) && index.column() != Column::Duration) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

auto ChapterTableModel::setData(const QModelIndex& index, const QVariant& value, const int role) -> bool {
    if (!index.isValid() || index.model() != this || index.row() < 0 || index.row() >= chapter_count()
        || index.column() < 0 || index.column() >= Column::Count || role != Qt::EditRole || is_append_row(index)) {
        return false;
    }

    auto& chapter = chapters_[static_cast<size_t>(index.row())];
    auto changed_column = Column::Name;
    switch (index.column()) {
    case Column::Name:
        chapter.name = trim_copy(qstring_to_utf8(value.toString()));
        changed_column = Column::Name;
        break;
    case Column::Start: {
        const auto parsed = parse_time(value.toString());
        if (!parsed.has_value()) {
            return false;
        }
        chapter.start_ms = *parsed;
        changed_column = Column::Start;
        break;
    }
    case Column::End: {
        const auto parsed = parse_time(value.toString());
        if (!parsed.has_value()) {
            return false;
        }
        chapter.end_ms = *parsed;
        changed_column = Column::End;
        break;
    }
    case Column::Duration:
    case Column::Count:
        return false;
    }

    const auto display_roles = QList<int> {Qt::DisplayRole, Qt::EditRole};
    if (changed_column == Column::Name) {
        emit dataChanged(this->index(index.row(), Column::Name), this->index(index.row(), Column::Name), display_roles);
    } else if (changed_column == Column::Start) {
        emit dataChanged(
            this->index(index.row(), Column::Start), this->index(index.row(), Column::Start), display_roles);
        emit dataChanged(
            this->index(index.row(), Column::Duration), this->index(index.row(), Column::Duration), display_roles);
        emit dataChanged(
            this->index(index.row(), Column::Name), this->index(index.row(), Column::Duration), {Qt::ForegroundRole});
    } else {
        emit dataChanged(
            this->index(index.row(), Column::End), this->index(index.row(), Column::Duration), display_roles);
        emit dataChanged(
            this->index(index.row(), Column::Name), this->index(index.row(), Column::Duration), {Qt::ForegroundRole});
    }
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

auto ChapterTableModel::chapter_count() const -> int {
    return static_cast<int>(chapters_.size());
}

auto ChapterTableModel::append_row_index() const -> int {
    return chapter_count();
}

auto ChapterTableModel::is_append_row(const QModelIndex& index) const -> bool {
    return index.isValid() && index.model() == this && index.row() == append_row_index() && index.column() >= 0
        && index.column() < Column::Count;
}

auto ChapterTableModel::set_display_mode(const TimestampDisplayMode mode) -> void {
    display_mode_ = mode;
    emit headerDataChanged(Qt::Horizontal, Column::Start, Column::End);
    if (chapter_count() > 0) {
        emit dataChanged(
            index(0, Column::Start), index(chapter_count() - 1, Column::Duration), {Qt::DisplayRole, Qt::EditRole});
    }
}

auto ChapterTableModel::set_frame_rate(const FrameRate frame_rate) -> void {
    frame_rate_ = frame_rate;
    if (display_mode_ == TimestampDisplayMode::Frames && chapter_count() > 0) {
        emit dataChanged(
            index(0, Column::Start), index(chapter_count() - 1, Column::Duration), {Qt::DisplayRole, Qt::EditRole});
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

    beginInsertRows(QModelIndex {}, chapter_count(), chapter_count());
    chapters_.push_back(ChapterSegment {
        .name = "Chapter " + std::to_string(chapters_.size() + 1),
        .start_ms = split_point,
        .end_ms = duration_ms,
    });
    endInsertRows();
    emit dataChanged(index(chapter_count() - 2, Column::End),
        index(chapter_count() - 2, Column::Duration),
        {Qt::DisplayRole, Qt::EditRole});
    emit dataChanged(
        index(chapter_count() - 2, Column::Name), index(chapter_count() - 2, Column::Duration), {Qt::ForegroundRole});
    emit chapters_changed();
    return true;
}

auto ChapterTableModel::remove_rows(const QModelIndexList& indices) -> void {
    auto rows = QSet<int> {};
    for (const auto& index : indices) {
        if (index.isValid() && index.model() == this && index.row() >= 0 && index.row() < chapter_count()) {
            rows.insert(index.row());
        }
    }

    auto sorted_rows = rows.values();
    std::ranges::sort(sorted_rows, std::greater {});

    for (const auto row : sorted_rows) {
        beginRemoveRows(QModelIndex {}, row, row);
        chapters_.erase(chapters_.begin() + row);
        endRemoveRows();
    }

    if (!sorted_rows.isEmpty()) {
        emit chapters_changed();
    }
}

auto ChapterTableModel::chapter_at(const int row) const -> const ChapterSegment* {
    if (row < 0 || row >= chapter_count()) {
        return nullptr;
    }

    return &chapters_[static_cast<size_t>(row)];
}

auto ChapterTableModel::format_time(const u64 milliseconds) const -> QString {
    return display_mode_ == TimestampDisplayMode::Frames
        ? utf8_to_qstring(format_frame_timecode(milliseconds, frame_rate_))
        : utf8_to_qstring(format_millisecond_timecode(milliseconds));
}

auto ChapterTableModel::parse_time(const QString& value) const -> std::optional<u64> {
    return display_mode_ == TimestampDisplayMode::Frames ? parse_frame_timecode(qstring_to_utf8(value), frame_rate_)
                                                         : parse_millisecond_timecode(qstring_to_utf8(value));
}

} // namespace vidchopper
