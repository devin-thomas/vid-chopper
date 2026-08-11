#include "qt/ui/presentation_controller.hpp"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QGuiApplication>
#include <QScreen>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>

namespace vidchopper {

PresentationController::PresentationController(QWidget& window, QObject* parent)
    : QObject(parent)
    , window_(&window) {
    if (qApp != nullptr) {
        base_font_point_size_ =
            (std::max)(10, static_cast<int>(qApp->font().pointSizeF() > 0.0 ? qApp->font().pointSizeF() : 10.0));
        qApp->installEventFilter(this);
    }
}

PresentationController::~PresentationController() {
    if (qApp != nullptr) {
        qApp->removeEventFilter(this);
    }
}

auto PresentationController::zoom_percent() const noexcept -> int {
    return zoom_percent_;
}

auto PresentationController::screen_size() const -> QSize {
    const auto* window_screen = window_ == nullptr ? nullptr : window_->screen();
    const auto* active_screen = window_screen != nullptr ? window_screen : QGuiApplication::primaryScreen();
    return active_screen == nullptr ? QSize {0, 0} : active_screen->geometry().size();
}

auto PresentationController::automatic_zoom_percent() const -> int {
    return ZoomPolicy::for_screen_height(screen_size().height());
}

auto PresentationController::apply_zoom_percent(const int zoom_percent, const bool persist) -> void {
    zoom_percent_ = ZoomPolicy::clamp(zoom_percent);

    if (qApp != nullptr) {
        auto font = qApp->font();
        font.setPointSizeF(static_cast<double>(base_font_point_size_) * static_cast<double>(zoom_percent_) / 100.0);
        qApp->setFont(font);

        const auto control_padding = (std::max)(6, zoom_percent_ / 20);
        const auto row_height = (std::max)(28, zoom_percent_ / 4);
        const auto section_padding = (std::max)(4, zoom_percent_ / 30);
        qApp->setStyleSheet(
            QStringLiteral("QPushButton, QToolButton { padding:%1px %2px; }"
                           "QLineEdit, QComboBox, QSpinBox { min-height:%3px; }"
                           "QHeaderView::section { padding:%4px; }"
                           "QProgressBar { min-height:%3px; }"
                           "QPlainTextEdit { font-family:'Cascadia Mono','Consolas','Courier New',monospace; }")
                .arg(control_padding)
                .arg(control_padding * 2)
                .arg(row_height)
                .arg(section_padding));
    }

    emit zoom_changed(zoom_percent_, persist);
}

auto PresentationController::zoom_in() -> void {
    apply_zoom_percent(zoom_percent_ + ZoomPolicy::step_percent, true);
}

auto PresentationController::zoom_out() -> void {
    apply_zoom_percent(zoom_percent_ - ZoomPolicy::step_percent, true);
}

auto PresentationController::reset_zoom() -> void {
    apply_zoom_percent(automatic_zoom_percent(), true);
}

auto PresentationController::eventFilter(QObject* watched, QEvent* event) -> bool {
    if (event->type() == QEvent::Wheel) {
        auto* wheel_event = static_cast<QWheelEvent*>(event);
        if (wheel_event->modifiers().testFlag(Qt::ControlModifier) && wheel_event->angleDelta().y() != 0) {
            if (wheel_event->angleDelta().y() > 0) {
                zoom_in();
            } else {
                zoom_out();
            }
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

} // namespace vidchopper
