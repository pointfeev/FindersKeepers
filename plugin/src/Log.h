#pragma once

#include <format>
#include <string_view>
#include <utility>

// Writes a line to the game log (kcd.log) under the plugin's prefix, where the in-game tests read their results. The
// format string is std::format's, checked against the arguments at compile time.
namespace Log {
    void WriteLine(std::string_view text);

    template <typename... Args> void Write(std::format_string<Args...> format, Args &&...args) {
        WriteLine(std::format(format, std::forward<Args>(args)...));
    }
} // namespace Log
