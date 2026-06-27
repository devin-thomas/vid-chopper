#include "qt/dark_palette.h"

#include <QApplication>
#include <QPalette>

namespace vidchopper {

auto apply_dark_palette(QApplication& app) -> void {
    auto palette = QPalette {};
    palette.setColor(QPalette::Window, QColor(24, 24, 28));
    palette.setColor(QPalette::WindowText, QColor(235, 235, 240));
    palette.setColor(QPalette::Base, QColor(16, 16, 18));
    palette.setColor(QPalette::AlternateBase, QColor(28, 28, 32));
    palette.setColor(QPalette::ToolTipBase, QColor(30, 30, 34));
    palette.setColor(QPalette::ToolTipText, QColor(240, 240, 245));
    palette.setColor(QPalette::Text, QColor(226, 226, 232));
    palette.setColor(QPalette::Button, QColor(38, 38, 44));
    palette.setColor(QPalette::ButtonText, QColor(230, 230, 236));
    palette.setColor(QPalette::BrightText, QColor(255, 120, 120));
    palette.setColor(QPalette::Link, QColor(120, 180, 255));
    palette.setColor(QPalette::Highlight, QColor(57, 92, 166));
    palette.setColor(QPalette::HighlightedText, QColor(246, 246, 250));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 110, 118));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(110, 110, 118));

    app.setPalette(palette);
    app.setStyleSheet(
        "QWidget { background-color: #18181c; color: #e6e6ec; }"
        "QLineEdit, QPlainTextEdit, QTableView, QComboBox, QSpinBox, QDoubleSpinBox, QTabWidget::pane {"
        "background-color: #101012; border: 1px solid #303038; border-radius: 4px; }"
        "QPushButton { background-color: #26262c; border: 1px solid #414149; padding: 6px 12px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #303038; }"
        "QPushButton:pressed { background-color: #383842; }"
        "QHeaderView::section { background-color: #202026; border: 0; padding: 6px; }"
        "QMenuBar::item:selected, QMenu::item:selected { background-color: #395ca6; }"
        "QProgressBar { background-color: #202026; border: 1px solid #303038; border-radius: 4px; text-align: center; }"
        "QProgressBar::chunk { background-color: #4f85f5; }"
        "QGroupBox { border: 1px solid #303038; border-radius: 6px; margin-top: 10px; padding-top: 10px; }"
        "QGroupBox::title { left: 10px; padding: 0 4px; }");
}

} // namespace vidchopper
