#include "ccm/util/SetNoNatural.hpp"

#include <cctype>

namespace ccm {

namespace {

[[nodiscard]] bool isAsciiDigit(char c) noexcept {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

[[nodiscard]] int cmpChar(char a, char b) noexcept {
    const auto ua = static_cast<unsigned char>(a);
    const auto ub = static_cast<unsigned char>(b);
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

}  // namespace

int compareSetNoNatural(std::string_view a, std::string_view b) noexcept {
    std::size_t i = 0;
    std::size_t j = 0;

    while (i < a.size() && j < b.size()) {
        const bool aDigit = isAsciiDigit(a[i]);
        const bool bDigit = isAsciiDigit(b[j]);

        if (aDigit && bDigit) {
            std::size_t aEnd = i;
            while (aEnd < a.size() && isAsciiDigit(a[aEnd])) ++aEnd;
            std::size_t bEnd = j;
            while (bEnd < b.size() && isAsciiDigit(b[bEnd])) ++bEnd;

            std::size_t aSig = i;
            while (aSig < aEnd && a[aSig] == '0') ++aSig;
            std::size_t bSig = j;
            while (bSig < bEnd && b[bSig] == '0') ++bSig;

            const std::size_t aLen = aEnd - aSig;
            const std::size_t bLen = bEnd - bSig;
            if (aLen != bLen) return aLen < bLen ? -1 : 1;

            for (std::size_t k = 0; k < aLen; ++k) {
                const int c = cmpChar(a[aSig + k], b[bSig + k]);
                if (c != 0) return c;
            }

            i = aEnd;
            j = bEnd;
            continue;
        }

        const int c = cmpChar(a[i], b[j]);
        if (c != 0) return c;
        ++i;
        ++j;
    }

    if (i == a.size() && j == b.size()) {
        if (a == b) return 0;
        return a < b ? -1 : 1;
    }
    return i == a.size() ? -1 : 1;
}

}  // namespace ccm
