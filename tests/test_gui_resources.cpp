#include "test_support.hpp"

#include <QCoreApplication>
#include <QImage>
#include <QSize>

auto main(int argc, char* argv[]) -> int {
    auto application = QCoreApplication {argc, argv};
    const auto icon_image = QImage {QStringLiteral(":/icons/app_icon.png")};

    test_support::expect_true(!icon_image.isNull(), "the application icon should be available from the Qt resource");
    test_support::expect_eq(icon_image.size(), QSize {256, 256}, "the packaged icon should retain its source size");
    test_support::expect_true(icon_image.hasAlphaChannel(), "the packaged icon should retain transparency");

    static_cast<void>(application);
    return 0;
}
