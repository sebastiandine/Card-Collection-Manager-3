#pragma once

// Natural (alphanumeric) ordering for collector / set numbers.
// Digit runs compare as integers so "2" < "10" < "100"; non-digit runs use
// ordinary string order (e.g. "SWSH001" < "SWSH002").

#include <string_view>

namespace ccm {

// strcmp-style: <0 if a < b, 0 if equal (after natural + lex tie-break), >0 if a > b.
[[nodiscard]] int compareSetNoNatural(std::string_view a, std::string_view b) noexcept;

}  // namespace ccm
