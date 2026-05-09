#pragma once

// MagicCard - faithful port of magic/card_services.rs::Card.
// JSON layout remains stable so collection.json files stay interchangeable.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ccm {

struct MagicCard {
    std::uint32_t id{0};
    std::uint8_t  amount{1};
    std::string   name;
    Set           set;
    std::string   note;
    std::vector<std::string> images;
    Language      language{Language::English};
    Condition     condition{Condition::NearMint};
    bool          foil{false};
    bool          signed_{false};   // `signed` is a reserved keyword
    bool          altered{false};

    friend bool operator==(const MagicCard&, const MagicCard&) = default;
};

void to_json(nlohmann::json& j, const MagicCard& c);
void from_json(const nlohmann::json& j, MagicCard& c);

}  // namespace ccm
