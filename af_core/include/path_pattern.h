/**
 * @file path_pattern.h
 * @brief Utility for converting REST path patterns into regular expressions
 *
 * Converts patterns such as "/sessions/{sessionId}/extend" (named
 * placeholders in curly braces) or "/sessions/* /extend" (bare wildcard
 * segments) into a regex string. Named and wildcard segments are each
 * turned into a capturing group that matches any run of characters
 * excluding '/'; all other characters are treated literally.
 *
 * The returned string is unanchored — callers should wrap it with '^' and
 * '$' (as RestRouter::extract_path_params does) before matching a full path.
 */

#pragma once

#include <string>

namespace af {
namespace communication {
namespace http {
namespace path_pattern {

inline std::string to_regex(const std::string& pattern) {
    static const std::string regex_special_chars = R"(.^$|()[]+?\)";

    std::string result;
    result.reserve(pattern.size() * 2);

    for (std::size_t i = 0; i < pattern.size(); ) {
        const char c = pattern[i];

        if (c == '{') {
            const auto end = pattern.find('}', i);
            if (end == std::string::npos) {
                result += "\\{";
                ++i;
                continue;
            }
            result += "([^/]+)";
            i = end + 1;
            continue;
        }

        if (c == '*') {
            result += "([^/]+)";
            ++i;
            continue;
        }

        if (regex_special_chars.find(c) != std::string::npos) {
            result += '\\';
        }
        result += c;
        ++i;
    }

    return result;
}

} // namespace path_pattern
} // namespace http
} // namespace communication
} // namespace af
