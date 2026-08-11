#pragma once

#include "qt/ui/zoom_policy.hpp"

#include <QObject>
#include <QPointer>
#include <QSize>

class QEvent;
class QWidget;

namespace vidchopper {

class PresentationController final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PresentationController)

public:
    explicit PresentationController(QWidget& window, QObject* parent = nullptr);
    ~PresentationController() override;

    [[nodiscard]] auto zoom_percent() const noexcept -> int;
    [[nodiscard]] auto screen_size() const -> QSize;
    [[nodiscard]] auto automatic_zoom_percent() const -> int;

    auto apply_zoom_percent(int zoom_percent, bool persist) -> void;
    auto zoom_in() -> void;
    auto zoom_out() -> void;
    auto reset_zoom() -> void;

signals:
    void zoom_changed(int zoom_percent, bool persist);

protected:
    auto eventFilter(QObject* watched, QEvent* event) -> bool override;

private:
    QPointer<QWidget> window_;
    int base_font_point_size_ {10};
    int zoom_percent_ {ZoomPolicy::default_percent};
};

} // namespace vidchopper
