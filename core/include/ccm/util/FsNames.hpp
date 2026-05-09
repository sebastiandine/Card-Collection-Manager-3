#pragma once

// Pure functions for filename munging. Ported from the original `util/fs.rs` so
// that file naming on disk stays identical between the two codebases.

#include <cstdint>
#include <string>
#include <string_view>

namespace ccm {

// Replace problematic characters so the result is safe to use as a filename
// component. Mirrors `format_text_for_fs` in Rust (drops apostrophes/commas,
// strips spaces, replaces `:` with `-`, `&` with `And`, `|` with `Or`,
// flattens accented vowels). Pure; safe at any call site.
std::string formatTextForFs(std::string_view text);

// Parse the trailing 1- or 2-digit numeric index from a filename. Examples:
//   "Image1.png"  -> 1
//   "Image22.jpeg" -> 22
// Returns 0 on parse failure, matching the optimistic Rust behavior.
std::uint8_t parseIndexFromFilename(std::string_view filename) noexcept;

}  // namespace ccm
