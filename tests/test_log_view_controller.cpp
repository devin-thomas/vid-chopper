#include "qt/ui/log_view_controller.hpp"
#include "test_support.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QWidget>

using namespace vidchopper;

auto main(int argc, char* argv[]) -> int {
    auto application = QApplication {argc, argv};
    auto container = QWidget {};
    auto toggle = QToolButton {&container};
    auto panel = QWidget {&container};
    auto advanced = QCheckBox {&panel};
    auto output = QPlainTextEdit {&panel};
    auto controller = LogViewController {toggle, panel, advanced, output, &container};

    test_support::expect_true(panel.isHidden(), "logs should start collapsed");
    controller.append(LogCategory::ProcessRaw, "frame=1");
    test_support::expect_true(output.toPlainText().isEmpty(), "curated logs should hide raw process output");

    controller.append(LogCategory::Probe, "Loaded C:/media/source.mp4");
    test_support::expect_true(output.toPlainText().contains("Loaded video: C:/media/source.mp4"),
        "curated logs should translate source loading");

    advanced.setChecked(true);
    test_support::expect_true(controller.advanced(), "advanced toggle should change the active filter");
    test_support::expect_true(output.toPlainText().startsWith("frame=1\nLoaded C:/media/source.mp4"),
        "filter changes should rebuild the complete retained view");

    controller.append(LogCategory::ProcessRaw, "frame=2");
    test_support::expect_true(
        output.toPlainText().endsWith("frame=2"), "active advanced logs should append new output incrementally");

    for (auto index = size_t {0}; index < LogViewController::maximum_entry_count + 20; ++index) {
        controller.append(LogCategory::App, QStringLiteral("retained-%1").arg(index));
    }
    test_support::expect_eq(
        controller.entry_count(), LogViewController::maximum_entry_count, "log entry retention should remain bounded");
    test_support::expect_true(controller.retained_character_count() <= LogViewController::maximum_total_characters,
        "log character retention should remain bounded");
    test_support::expect_true(
        output.toPlainText().contains(
            QStringLiteral("retained-%1").arg(static_cast<qulonglong>(LogViewController::maximum_entry_count + 19))),
        "the newest retained message should remain visible");
    test_support::expect_true(!output.toPlainText().contains("frame=1"),
        "entries evicted from bounded storage should also leave the active view");

    advanced.setChecked(false);
    test_support::expect_true(!output.toPlainText().contains("frame=2"),
        "switching back to curated logs should remove retained raw process output");

    toggle.setChecked(true);
    test_support::expect_true(!panel.isHidden(), "the disclosure controller should expand the log panel");

    static_cast<void>(application);
    return 0;
}
