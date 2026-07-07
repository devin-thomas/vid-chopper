#include "core/string_utils.h"
#include "test_support.h"

#include <cstddef>

using namespace vidchopper;

auto main() -> int {
    test_support::expect_eq(trim_copy("  hello  "), std::string {"hello"}, "trim should remove surrounding whitespace");
    test_support::expect_eq(replace_all_copy("%name% - %name%", "%name%", "clip"),
        std::string {"clip - clip"},
        "replace should update all placeholders");
    test_support::expect_eq(sanitize_file_component("  Intro: Setup?  "),
        std::string {"Intro_ Setup_"},
        "sanitize should replace reserved characters");
    test_support::expect_eq(zero_padded_index(7, 3), std::string {"007"}, "index helper should pad values");

    const auto tokens = split_quoted_arguments("-movflags +faststart \"-metadata comment=hello world\"");
    test_support::expect_eq(tokens.size(), std::size_t {3}, "quoted arguments should preserve grouped tokens");
    test_support::expect_eq(
        tokens.back(), std::string {"-metadata comment=hello world"}, "quoted token should keep spaces");

    return 0;
}
