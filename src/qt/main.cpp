#include "qt/app_settings.hpp"
#include "qt/dark_palette.hpp"
#include "qt/demo_launch_options.hpp"
#include "qt/ui/main_window.hpp"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QStringList>

#include <iostream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] auto parse_demo_arguments(const QStringList& arguments) -> vidchopper::DemoLaunchOptionsParseResult {
    auto utf8_arguments = std::vector<std::string> {};
    utf8_arguments.reserve(static_cast<size_t>(arguments.size()));
    for (const auto& argument : arguments) {
        const auto encoded = argument.toUtf8();
        utf8_arguments.emplace_back(encoded.constData(), static_cast<size_t>(encoded.size()));
    }

    auto argv = std::vector<char*> {};
    argv.reserve(utf8_arguments.size());
    for (auto& argument : utf8_arguments) {
        argv.push_back(argument.data());
    }

    return vidchopper::parse_demo_launch_options(static_cast<int>(argv.size()), argv.data());
}

} // namespace

auto main(int argc, char* argv[]) -> int {
    // Qt retains the native Windows command line as Unicode. Convert that canonical list to UTF-8
    // before the shared parser turns demo paths into std::filesystem::path values.
    auto app = QApplication {argc, argv};
    const auto parsed_demo_options = parse_demo_arguments(QApplication::arguments());
    if (!parsed_demo_options.success) {
        std::cerr << parsed_demo_options.error_message << '\n';
        return 1;
    }

    QApplication::setApplicationName("VidChopper");
    QApplication::setOrganizationName("Devin Thomas");
    QApplication::setOrganizationDomain("github.com/devin-thomas");
    QApplication::setApplicationVersion(QStringLiteral(VIDCHOPPER_DISPLAY_VERSION));
    QApplication::setWindowIcon(QIcon(":/icons/app_icon.png"));
    app.setStyle(QStyleFactory::create("Fusion"));

    vidchopper::apply_dark_palette(app);

    auto window = vidchopper::MainWindow {parsed_demo_options.options};
    window.show();
    return app.exec();
}
