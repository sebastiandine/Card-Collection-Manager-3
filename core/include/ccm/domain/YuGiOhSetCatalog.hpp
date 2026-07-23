#pragma once

// YuGiOhSetCatalog: offline pack → card checklist for Yu-Gi-Oh! set
// completion. Filled from YGOPRODeck cardinfo.php (all-cards dump) and
// persisted at `<dataStorage>/yugioh/set-catalog.json`.

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct YuGiOhCatalogCard {
    std::string setNo;
    std::string name;

    friend bool operator==(const YuGiOhCatalogCard&,
                           const YuGiOhCatalogCard&) = default;
};

struct YuGiOhSetCatalogPack {
    std::string setId;
    std::string setName;
    std::vector<YuGiOhCatalogCard> cards;

    friend bool operator==(const YuGiOhSetCatalogPack&,
                           const YuGiOhSetCatalogPack&) = default;
};

struct YuGiOhSetCatalog {
    std::vector<YuGiOhSetCatalogPack> packs;

    [[nodiscard]] const YuGiOhSetCatalogPack* findPack(
        std::string_view setId) const;

    [[nodiscard]] bool empty() const noexcept { return packs.empty(); }

    friend bool operator==(const YuGiOhSetCatalog&,
                           const YuGiOhSetCatalog&) = default;
};

void to_json(nlohmann::json& j, const YuGiOhCatalogCard& c);
void from_json(const nlohmann::json& j, YuGiOhCatalogCard& c);
void to_json(nlohmann::json& j, const YuGiOhSetCatalogPack& p);
void from_json(const nlohmann::json& j, YuGiOhSetCatalogPack& p);
void to_json(nlohmann::json& j, const YuGiOhSetCatalog& c);
void from_json(const nlohmann::json& j, YuGiOhSetCatalog& c);

}  // namespace ccm
