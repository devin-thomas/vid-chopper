#pragma once

#include "core/types.hpp"
#include "qt/logging.hpp"

#include <QObject>
#include <QPointer>

#include <deque>

class QCheckBox;
class QPlainTextEdit;
class QToolButton;
class QWidget;

namespace vidchopper {

class LogViewController final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LogViewController)

public:
    static constexpr auto maximum_entry_count = size_t {2000};
    static constexpr auto maximum_entry_characters = qsizetype {64 * 1024};
    static constexpr auto maximum_total_characters = qsizetype {512 * 1024};

    LogViewController(QToolButton& toggle_button,
        QWidget& panel,
        QCheckBox& advanced_checkbox,
        QPlainTextEdit& output,
        QObject* parent = nullptr);

    [[nodiscard]] auto entry_count() const noexcept -> size_t;
    [[nodiscard]] auto retained_character_count() const noexcept -> qsizetype;
    [[nodiscard]] auto advanced() const noexcept -> bool;

    auto set_expanded(bool expanded) -> void;
    auto set_advanced(bool advanced) -> void;

public slots:
    void append(LogCategory category, const QString& message);

private:
    [[nodiscard]] auto bounded_message(const QString& message) const -> QString;
    [[nodiscard]] auto visible_message(const LogEntry& entry) const -> QString;
    auto append_to_view(const QString& message) -> void;
    auto remove_from_view(const LogEntry& entry) -> void;
    auto rebuild_view() -> void;
    auto scroll_to_end() -> void;

    QPointer<QToolButton> toggle_button_;
    QPointer<QWidget> panel_;
    QPointer<QCheckBox> advanced_checkbox_;
    QPointer<QPlainTextEdit> output_;
    std::deque<LogEntry> entries_;
    qsizetype retained_character_count_ {0};
    qsizetype visible_character_count_ {0};
    bool advanced_ {false};
};

} // namespace vidchopper
