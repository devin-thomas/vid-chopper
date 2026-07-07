#include "core/string_utils.h"
#include "test_support.h"

#include <cstddef>

using namespace vidchopper;

auto main() -> int {
    // trim_copy: various edge cases
    {
        test_support::expect_eq(trim_copy(""), std::string {""}, "empty string trims to empty");
        test_support::expect_eq(trim_copy("   "), std::string {""}, "whitespace-only trims to empty");
        test_support::expect_eq(trim_copy("hello"), std::string {"hello"}, "no whitespace remains unchanged");
        test_support::expect_eq(trim_copy("\t\n hello \r\n"), std::string {"hello"}, "tabs and newlines are trimmed");
        test_support::expect_eq(trim_copy("  a b c  "), std::string {"a b c"}, "interior whitespace is preserved");
    }

    // replace_all_copy: edge cases
    {
        test_support::expect_eq(
            replace_all_copy("hello", "", "world"), std::string {"hello"}, "empty 'from' should return original");
        test_support::expect_eq(replace_all_copy("", "a", "b"), std::string {""}, "empty input returns empty");
        test_support::expect_eq(
            replace_all_copy("aaa", "a", "bb"), std::string {"bbbbbb"}, "multiple consecutive replacements");
        test_support::expect_eq(
            replace_all_copy("abc", "abc", ""), std::string {""}, "replacing entire string with empty");
        test_support::expect_eq(
            replace_all_copy("no match", "xyz", "replaced"), std::string {"no match"}, "no match returns original");
        test_support::expect_eq(replace_all_copy("a.b.c", ".", "-"), std::string {"a-b-c"}, "dot replacement");
    }

    // split_quoted_arguments: edge cases
    {
        const auto empty_result = split_quoted_arguments("");
        test_support::expect_eq(empty_result.size(), std::size_t {0}, "empty input yields no tokens");

        const auto whitespace_result = split_quoted_arguments("   ");
        test_support::expect_eq(whitespace_result.size(), std::size_t {0}, "whitespace-only yields no tokens");

        const auto single = split_quoted_arguments("single");
        test_support::expect_eq(single.size(), std::size_t {1}, "single word yields one token");
        test_support::expect_eq(single[0], std::string {"single"}, "single word content");

        const auto multi_space = split_quoted_arguments("a   b   c");
        test_support::expect_eq(multi_space.size(), std::size_t {3}, "multiple spaces between tokens");

        const auto quoted_empty = split_quoted_arguments("\"\"");
        test_support::expect_eq(quoted_empty.size(), std::size_t {0}, "empty quoted string yields no tokens");

        const auto mixed = split_quoted_arguments("before \"quoted part\" after");
        test_support::expect_eq(mixed.size(), std::size_t {3}, "mixed quoted/unquoted should yield 3 tokens");
        test_support::expect_eq(mixed[1], std::string {"quoted part"}, "quoted token preserves spaces");

        const auto adjacent_quotes = split_quoted_arguments("\"first\"\"second\"");
        test_support::expect_eq(
            adjacent_quotes.size(), std::size_t {1}, "adjacent quoted strings merge into one token");
        test_support::expect_eq(adjacent_quotes[0], std::string {"firstsecond"}, "adjacent quotes merge content");
    }

    // sanitize_file_component: forbidden characters
    {
        test_support::expect_eq(sanitize_file_component("normal"), std::string {"normal"}, "clean input unchanged");
        test_support::expect_eq(sanitize_file_component("a<b>c"), std::string {"a_b_c"}, "angle brackets replaced");
        test_support::expect_eq(sanitize_file_component("a:b"), std::string {"a_b"}, "colon replaced");
        test_support::expect_eq(sanitize_file_component("a\"b"), std::string {"a_b"}, "double quote replaced");
        test_support::expect_eq(sanitize_file_component("a/b\\c"), std::string {"a_b_c"}, "slashes replaced");
        test_support::expect_eq(sanitize_file_component("a|b"), std::string {"a_b"}, "pipe replaced");
        test_support::expect_eq(
            sanitize_file_component("a?b*c"), std::string {"a_b_c"}, "question mark and asterisk replaced");
    }

    // sanitize_file_component: control characters stripped
    {
        auto input = std::string {"hello"};
        input.insert(input.begin() + 2, '\x01');
        input.insert(input.begin() + 4, '\x1F');
        const auto result = sanitize_file_component(input);
        test_support::expect_eq(result, std::string {"hello"}, "control characters should be stripped");
    }

    // sanitize_file_component: consecutive spaces collapsed
    {
        test_support::expect_eq(
            sanitize_file_component("a   b   c"), std::string {"a b c"}, "consecutive spaces should be collapsed");
    }

    // sanitize_file_component: trailing dots removed
    {
        test_support::expect_eq(
            sanitize_file_component("file..."), std::string {"file"}, "trailing dots should be removed");
        test_support::expect_eq(
            sanitize_file_component("file. ."), std::string {"file"}, "trailing dots and spaces should be removed");
    }

    // sanitize_file_component: empty / all-forbidden input
    {
        test_support::expect_eq(
            sanitize_file_component(""), std::string {"chapter"}, "empty input should default to 'chapter'");
        test_support::expect_eq(
            sanitize_file_component("   "), std::string {"chapter"}, "whitespace-only should default to 'chapter'");
        test_support::expect_eq(
            sanitize_file_component("???"), std::string {"___"}, "all-forbidden non-whitespace should be replaced");
    }

    // sanitize_file_component: surrounding whitespace trimmed
    {
        test_support::expect_eq(
            sanitize_file_component("  hello  "), std::string {"hello"}, "surrounding whitespace should be trimmed");
    }

    // zero_padded_index: various widths and values
    {
        test_support::expect_eq(zero_padded_index(1, 2), std::string {"01"}, "width 2, value 1");
        test_support::expect_eq(zero_padded_index(10, 2), std::string {"10"}, "width 2, value 10");
        test_support::expect_eq(zero_padded_index(100, 2), std::string {"100"}, "value exceeds width stays as-is");
        test_support::expect_eq(zero_padded_index(0, 3), std::string {"000"}, "zero padded to width 3");
        test_support::expect_eq(zero_padded_index(42, 4), std::string {"0042"}, "width 4, value 42");
        test_support::expect_eq(zero_padded_index(255, 1), std::string {"255"}, "max u8 width with large value");
    }

    return 0;
}
