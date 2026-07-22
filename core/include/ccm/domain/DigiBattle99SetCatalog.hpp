#pragma once

// DigiBattle99SetCatalog: offline pack → card checklist for Digi-Battle set
// completion. Filled from digimoncard.io bulk search.php (same payload as the
// set list) and persisted at `<dataStorage>/digibattle99/set-catalog.json`.

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct DigiBattle99CatalogCard {
    std::string setNo;
    std::string name;

    friend bool operator==(const DigiBattle99CatalogCard&,
                           const DigiBattle99CatalogCard&) = default;
};

struct DigiBattle99SetCatalogPack {
    std::string setId;
    std::string setName;
    std::vector<DigiBattle99CatalogCard> cards;

    friend bool operator==(const DigiBattle99SetCatalogPack&,
                           const DigiBattle99SetCatalogPack&) = default;
};

struct DigiBattle99SetCatalog {
    std::vector<DigiBattle99SetCatalogPack> packs;

    [[nodiscard]] const DigiBattle99SetCatalogPack* findPack(
        std::string_view setId) const;

    [[nodiscard]] bool empty() const noexcept { return packs.empty(); }

    friend bool operator==(const DigiBattle99SetCatalog&,
                           const DigiBattle99SetCatalog&) = default;
};

void to_json(nlohmann::json& j, const DigiBattle99CatalogCard& c);
void from_json(const nlohmann::json& j, DigiBattle99CatalogCard& c);
void to_json(nlohmann::json& j, const DigiBattle99SetCatalogPack& p);
void from_json(const nlohmann::json& j, DigiBattle99SetCatalogPack& p);
void to_json(nlohmann::json& j, const DigiBattle99SetCatalog& c);
void from_json(const nlohmann::json& j, DigiBattle99SetCatalog& c);

}  // namespace ccm
