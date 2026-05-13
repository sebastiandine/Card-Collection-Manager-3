#pragma once

// Tracks when a modal Add/Edit card dialog is on screen so a second one
// cannot be stacked (toolbar + list activation, or rare re-entrant cases).

#include <atomic>

namespace ccm::ui {

// User-visible hint when Add/Edit is requested while a card dialog is already modal.
inline constexpr const char* kCardEditModalBlockedUtf8 =
    "Close the open card dialog (save or cancel) before opening another card.";

[[nodiscard]] inline std::atomic<int>& cardEditModalDepthRef() noexcept {
    static std::atomic<int> depth{0};
    return depth;
}

[[nodiscard]] inline bool cardEditModalIsActive() noexcept {
    return cardEditModalDepthRef().load(std::memory_order_relaxed) > 0;
}

struct CardEditModalGuard {
    CardEditModalGuard() {
        cardEditModalDepthRef().fetch_add(1, std::memory_order_relaxed);
    }
    ~CardEditModalGuard() {
        cardEditModalDepthRef().fetch_sub(1, std::memory_order_relaxed);
    }
    CardEditModalGuard(const CardEditModalGuard&)            = delete;
    CardEditModalGuard& operator=(const CardEditModalGuard&) = delete;
};

}  // namespace ccm::ui
