#include "cli/cli_app.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace vidchopper;

auto main(int argc, char* argv[]) -> int {
    auto arguments = std::vector<std::string> {};
    arguments.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    const auto request = CliRunRequest {
        .arguments = std::move(arguments),
        .executable_path = argc > 0 ? Path {argv[0]} : Path {},
        .output = std::cout,
        .error_output = std::cerr,
    };

    return static_cast<int>(run_cli(request));
}
