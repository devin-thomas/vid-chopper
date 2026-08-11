#include "qt/ui/log_view_controller.hpp"

#include <QCheckBox>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QWidget>

#include <utility>

namespace vidchopper {

namespace {

[[nodiscard]] auto translated_log_message(const LogEntry& entry) -> QString {
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

    return entry.message;
}

} // namespace

LogViewController::LogViewController(QToolButton& toggle_button,
    QWidget& panel,
    QCheckBox& advanced_checkbox,
    QPlainTextEdit& output,
    QObject* parent)
    : QObject(parent)
    , toggle_button_(&toggle_button)
    , panel_(&panel)
    , advanced_checkbox_(&advanced_checkbox)
    , output_(&output) {
    output_->setUndoRedoEnabled(false);
    connect(toggle_button_, &QToolButton::toggled, this, &LogViewController::set_expanded);
    connect(advanced_checkbox_, &QCheckBox::toggled, this, &LogViewController::set_advanced);
    set_expanded(false);
}

auto LogViewController::entry_count() const noexcept -> size_t {
    return entries_.size();
}

auto LogViewController::retained_character_count() const noexcept -> qsizetype {
    return retained_character_count_;
}

auto LogViewController::advanced() const noexcept -> bool {
    return advanced_;
}

auto LogViewController::set_expanded(const bool expanded) -> void {
    if (toggle_button_ == nullptr || panel_ == nullptr) {
        return;
    }

    const auto blocker = QSignalBlocker {*toggle_button_};
    toggle_button_->setChecked(expanded);
    toggle_button_->setText(expanded ? QStringLiteral("\u25be Hide Logs") : QStringLiteral("\u25b8 Show Logs"));
    panel_->setVisible(expanded);
}

auto LogViewController::set_advanced(const bool advanced) -> void {
    if (advanced_ == advanced) {
        return;
    }

    advanced_ = advanced;
    if (advanced_checkbox_ != nullptr && advanced_checkbox_->isChecked() != advanced) {
        const auto blocker = QSignalBlocker {*advanced_checkbox_};
        advanced_checkbox_->setChecked(advanced);
    }
    rebuild_view();
}

void LogViewController::append(const LogCategory category, const QString& message) {
    entries_.push_back(LogEntry {
        .category = category,
        .message = bounded_message(message),
    });
    retained_character_count_ += entries_.back().message.size();

    append_to_view(visible_message(entries_.back()));

    while (entries_.size() > maximum_entry_count || retained_character_count_ > maximum_total_characters) {
        remove_from_view(entries_.front());
        retained_character_count_ -= entries_.front().message.size();
        entries_.pop_front();
    }
}

auto LogViewController::bounded_message(const QString& message) const -> QString {
    if (message.size() <= maximum_entry_characters) {
        return message;
    }

    const auto marker = QStringLiteral("\n[message truncated]");
    return message.left(maximum_entry_characters - marker.size()) + marker;
}

auto LogViewController::visible_message(const LogEntry& entry) const -> QString {
    return advanced_ ? entry.message : translated_log_message(entry);
}

auto LogViewController::append_to_view(const QString& message) -> void {
    if (message.isEmpty() || output_ == nullptr) {
        return;
    }

    auto cursor = QTextCursor {output_->document()};
    cursor.movePosition(QTextCursor::End);
    if (visible_character_count_ > 0) {
        cursor.insertText("\n");
        ++visible_character_count_;
    }
    cursor.insertText(message);
    visible_character_count_ += message.size();
    output_->setTextCursor(cursor);
    scroll_to_end();
}

auto LogViewController::remove_from_view(const LogEntry& entry) -> void {
    const QString message = visible_message(entry);
    if (message.isEmpty() || output_ == nullptr || visible_character_count_ == 0) {
        return;
    }

    auto removal_length = message.size();
    if (visible_character_count_ > removal_length) {
        ++removal_length;
    }

    auto cursor = QTextCursor {output_->document()};
    cursor.setPosition(0);
    cursor.setPosition(static_cast<int>(removal_length), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    visible_character_count_ -= removal_length;
}

auto LogViewController::rebuild_view() -> void {
    if (output_ == nullptr) {
        return;
    }

    auto text = QString {};
    text.reserve(retained_character_count_);
    for (const LogEntry& entry : entries_) {
        const QString message = visible_message(entry);
        if (message.isEmpty()) {
            continue;
        }
        if (!text.isEmpty()) {
            text += '\n';
        }
        text += message;
    }

    output_->setPlainText(text);
    visible_character_count_ = text.size();
    scroll_to_end();
}

auto LogViewController::scroll_to_end() -> void {
    if (output_ == nullptr) {
        return;
    }

    if (auto* scrollbar = output_->verticalScrollBar(); scrollbar != nullptr) {
        scrollbar->setValue(scrollbar->maximum());
    }
}

} // namespace vidchopper
