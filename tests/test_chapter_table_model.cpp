#include "qt/ui/chapter_table_model.hpp"
#include "test_support.hpp"

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QSignalSpy>

#include <vector>

using namespace vidchopper;

auto main(int argc, char* argv[]) -> int {
    auto application = QCoreApplication {argc, argv};
    auto model = ChapterTableModel {};
    auto tester = QAbstractItemModelTester {&model, QAbstractItemModelTester::FailureReportingMode::Fatal};

    model.set_chapters({
        {.name = "Intro", .start_ms = 0, .end_ms = 2000},
        {.name = "Match", .start_ms = 2000, .end_ms = 4000},
    });
    test_support::expect_eq(model.chapter_count(), 2, "reset should install the chapter plan");
    test_support::expect_eq(model.rowCount(), 3, "model should include its append row");

    auto changed = QSignalSpy {&model, &QAbstractItemModel::dataChanged};
    const bool renamed = model.setData(model.index(0, 0), QStringLiteral(" Opening "), Qt::EditRole);
    test_support::expect_true(renamed, "owned editable index should update");
    test_support::expect_eq(model.chapters().front().name, std::string {"Opening"}, "name edit should be trimmed");
    test_support::expect_eq(changed.count(), 1, "name edit should emit one precise change");
    const QList<QVariant> rename_arguments = changed.takeFirst();
    test_support::expect_eq(
        qvariant_cast<QModelIndex>(rename_arguments[0]).column(), 0, "name edit should begin at the name column");
    test_support::expect_eq(
        qvariant_cast<QModelIndex>(rename_arguments[1]).column(), 0, "name edit should end at the name column");
    const QList<int> rename_roles = qvariant_cast<QList<int>>(rename_arguments[2]);
    test_support::expect_true(rename_roles.contains(Qt::DisplayRole) && rename_roles.contains(Qt::EditRole),
        "name edit should identify display and edit roles");

    model.set_frame_rate(FrameRate {.numerator = 60, .denominator = 1});
    model.set_display_mode(TimestampDisplayMode::Frames);
    test_support::expect_true(
        model.data(model.index(0, 1)).toString().endsWith(":00"), "frame display mode should refresh time columns");

    const bool appended = model.append_chapter(4000);
    test_support::expect_true(appended, "append should split a sufficiently long final chapter");
    test_support::expect_eq(model.chapter_count(), 3, "append should add one chapter");

    model.remove_rows({model.index(1, 0)});
    test_support::expect_eq(model.chapter_count(), 2, "owned row removal should update the model");

    auto foreign_model = ChapterTableModel {};
    foreign_model.set_chapters({{.name = "Foreign", .start_ms = 0, .end_ms = 2000}});
    const QModelIndex foreign_index = foreign_model.index(0, 0);
    test_support::expect_true(!model.setData(foreign_index, QStringLiteral("Wrong model"), Qt::EditRole),
        "public mutation should reject a foreign model index");
    model.remove_rows({foreign_index});
    test_support::expect_eq(model.chapter_count(), 2, "foreign removal index should not mutate the model");
    test_support::expect_true(!model.is_append_row(foreign_model.index(foreign_model.append_row_index(), 0)),
        "append-row query should reject a foreign model index");

    static_cast<void>(application);
    static_cast<void>(tester);
    return 0;
}
