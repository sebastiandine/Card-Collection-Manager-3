#pragma once

// YuGiOhBandaiCard - Bandai Carddass (pre-Konami) card model.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ccm {

struct YuGiOhBandaiCard {
    std::uint32_t id{0};
    std::uint8_t  amount{1};
    std::string   name;
    Set           set;
    std::string   setNo;
    std::string   rarity;
    std::string   note;
    std::vector<std::string> images;
    Language      language{Language::Japanese};
    Condition     condition{Condition::NearMint};
    bool          holo{false};
    bool          signed_{false};
    bool          altered{false};

    friend bool operator==(const YuGiOhBandaiCard&, const YuGiOhBandaiCard&) = default;
};

void to_json(nlohmann::json& j, const YuGiOhBandaiCard& c);
void from_json(const nlohmann::json& j, YuGiOhBandaiCard& c);

}  // namespace ccm
