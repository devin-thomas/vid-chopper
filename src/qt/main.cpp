#include "qt/app_settings.h"
#include "qt/dark_palette.h"
#include "qt/ui/main_window.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

auto main(int argc, char* argv[]) -> int {
    auto app = QApplication {argc, argv};
    QApplication::setApplicationName("VidChopper");
    QApplication::setOrganizationName("Devin Thomas");
    QApplication::setOrganizationDomain("github.com/devin-thomas");
    QApplication::setWindowIcon(QIcon(":/icons/app_icon.ico"));
    app.setStyle(QStyleFactory::create("Fusion"));

    vidchopper::apply_dark_palette(app);

    auto window = vidchopper::MainWindow {};
    window.show();
    return app.exec();
}
