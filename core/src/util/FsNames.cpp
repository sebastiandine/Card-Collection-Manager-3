#include "ccm/util/FsNames.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace ccm {

namespace {

// Replacements ordered exactly like the Rust source's chained .replace(...) so
// behavior is bit-identical for inputs that contain multiple of these chars.
struct Replacement {
    std::string_view from;
    std::string_view to;
};

constexpr std::array<Replacement, 14> kReplacements{{
    {"'",  ""},
    {"`",  ""},
    {",",  ""},
    {" ",  ""},
    {":",  "-"},
    {"&",  "And"},
    {"|",  "Or"},
    {"\xC3\xA1", "a"},  // a-acute  (UTF-8)
    {"\xC3\xA9", "e"},  // e-acute
    {"\xC3\xAD", "i"},  // i-acute
    {"\xC3\xB3", "o"},  // o-acute
    {"\xC3\xBA", "u"},  // u-acute
    {"\xC3\xBB", "u"},  // u-circumflex
    // Remaining accented vowels appear in modern Scryfall data but were not
    // listed in the Rust source. Keeping behavior 1:1 deliberately.
}};

void replaceAllInPlace(std::string& s, std::string_view from, std::string_view to) {
    if (from.empty()) return;
    std::string::size_type pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

}  // namespace

std::string formatTextForFs(std::string_view text) {
    std::string out(text);
    for (const auto& r : kReplacements) {
        replaceAllInPlace(out, r.from, r.to);
    }
    return out;
}

std::uint8_t parseIndexFromFilename(std::string_view filename) noexcept {
    const auto dot = filename.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0) return 0;

    // Walk backwards from the position before the dot, collecting digits.
    std::size_t end = dot;
    std::size_t begin = end;
    while (begin > 0) {
        unsigned char ch = static_cast<unsigned char>(filename[begin - 1]);
        if (std::isdigit(ch)) {
            --begin;
            // Rust source intentionally caps at 2-digit indices.
            if (end - begin >= 2) break;
        } else {
            break;
        }
    }
    if (begin == end) return 0;

    unsigned int value = 0;
    for (std::size_t i = begin; i < end; ++i) {
        value = value * 10 + static_cast<unsigned int>(filename[i] - '0');
    }
    return static_cast<std::uint8_t>(value);
}

}  // namespace ccm
