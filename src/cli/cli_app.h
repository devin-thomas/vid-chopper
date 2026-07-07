#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace vidchopper {

[[nodiscard]] auto run_cli(const std::vector<std::string>& arguments,
    const std::filesystem::path& executable_path,
    std::ostream& output,
    std::ostream& error_output) -> int;

} // namespace vidchopper
