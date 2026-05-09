#pragma once

// PokemonCard - faithful port of pokemon/card_services.rs::Card.
// Same established JSON shape (with `setNo` and `firstEdition` aliases).

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ccm {

struct PokemonCard {
    std::uint32_t id{0};
    std::uint8_t  amount{1};
    std::string   name;
    Set           set;
    std::string   setNo;
    std::string   note;
    std::vector<std::string> images;
    Language      language{Language::English};
    Condition     condition{Condition::NearMint};
    bool          firstEdition{false};
    bool          holo{false};
    bool          signed_{false};
    bool          altered{false};

    friend bool operator==(const PokemonCard&, const PokemonCard&) = default;
};

void to_json(nlohmann::json& j, const PokemonCard& c);
void from_json(const nlohmann::json& j, PokemonCard& c);

}  // namespace ccm
