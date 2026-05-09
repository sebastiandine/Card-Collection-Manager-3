#pragma once

// YuGiOhCard - Yu-Gi-Oh card model with print-level metadata.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ccm {

struct YuGiOhCard {
    std::uint32_t id{0};
    std::uint8_t  amount{1};
    std::string   name;
    Set           set;
    std::string   setNo;
    std::string   rarity;
    std::string   rarityCode;
    std::string   note;
    std::vector<std::string> images;
    Language      language{Language::English};
    Condition     condition{Condition::NearMint};
    bool          firstEdition{false};
    bool          signed_{false};
    bool          altered{false};

    friend bool operator==(const YuGiOhCard&, const YuGiOhCard&) = default;
};

void to_json(nlohmann::json& j, const YuGiOhCard& c);
void from_json(const nlohmann::json& j, YuGiOhCard& c);

}  // namespace ccm
