#pragma once

// JapanesePokemonCard - Japanese Pokémon TCG collection model.
// Pokémon-shaped field set (setNo / holo / firstEdition / signed / altered).

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ccm {

struct JapanesePokemonCard {
    std::uint32_t id{0};
    std::uint8_t  amount{1};
    std::string   name;
    Set           set;
    std::string   setNo;
    std::string   note;
    std::vector<std::string> images;
    Language      language{Language::Japanese};
    Condition     condition{Condition::NearMint};
    bool          firstEdition{false};
    bool          holo{false};
    bool          signed_{false};
    bool          altered{false};

    friend bool operator==(const JapanesePokemonCard&, const JapanesePokemonCard&) = default;
};

void to_json(nlohmann::json& j, const JapanesePokemonCard& c);
void from_json(const nlohmann::json& j, JapanesePokemonCard& c);

}  // namespace ccm
