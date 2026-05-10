#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace ccm {

// ASCII-only tolower for sort/filter parity with the legacy TS path:
// String.prototype.toLowerCase() on English/German/etc. card metadata behaves
// identically for this byte range.
[[nodiscard]] inline std::string asciiLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

}  // namespace ccm
