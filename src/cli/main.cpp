#include "cli/cli_app.h"

#include <iostream>
#include <string>
#include <vector>

using namespace vidchopper;

auto main(int argc, char* argv[]) -> int {
    auto arguments = std::vector<std::string> {};
    arguments.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (auto index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    const auto executable_path = argc > 0 ? std::filesystem::path {argv[0]} : std::filesystem::path {};
    return run_cli(arguments, executable_path, std::cout, std::cerr);
}
