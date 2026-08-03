#pragma once

// Shared rule for bidirectional Set # Auto detect (name ↔ set number):
// when both fields are filled, the field the user last edited is the lookup key.

namespace ccm {

enum class CardLookupEditField { None, Name, SetNo };

// Returns true when Auto detect should run setNo → name (reverse).
// `nameEmpty` / `setNoEmpty` are already trimmed/normalized by the caller.
// When both are empty the result is false (caller shows a validation message).
// When only one is filled, that direction wins. When both are filled, SetNo
// wins only if it was the last edited lookup field; otherwise Name wins
// (including `None`, matching the historical default).
[[nodiscard]] inline bool preferDetectBySetNo(bool nameEmpty,
                                              bool setNoEmpty,
                                              CardLookupEditField lastEdited) noexcept {
    if (nameEmpty) return !setNoEmpty;
    if (setNoEmpty) return false;
    return lastEdited == CardLookupEditField::SetNo;
}

}  // namespace ccm
