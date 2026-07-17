#include "qt/app_settings.hpp"
#include "qt/dark_palette.hpp"
#include "qt/demo_launch_options.hpp"
#include "qt/ui/main_window.hpp"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

#include <iostream>

auto main(int argc, char* argv[]) -> int {
    const auto parsed_demo_options = vidchopper::parse_demo_launch_options(argc, argv);
    if (!parsed_demo_options.success) {
        std::cerr << parsed_demo_options.error_message << '\n';
        return 1;
    }

    auto app = QApplication {argc, argv};
    QApplication::setApplicationName("VidChopper");
    QApplication::setOrganizationName("Devin Thomas");
    QApplication::setOrganizationDomain("github.com/devin-thomas");
    QApplication::setWindowIcon(QIcon(":/icons/app_icon.ico"));
    app.setStyle(QStyleFactory::create("Fusion"));

    vidchopper::apply_dark_palette(app);

    auto window = vidchopper::MainWindow {parsed_demo_options.options};
    window.show();
    return app.exec();
}
