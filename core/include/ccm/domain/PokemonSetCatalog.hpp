#pragma once

// PokemonSetCatalog: offline pack → card checklist for Pokemon set
// completion. West and Asia each persist their own file under
// `<dataStorage>/pokemon/` (`set-catalog-west.json` / `set-catalog-asia.json`).

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct PokemonCatalogCard {
    std::string setNo;
    std::string name;

    friend bool operator==(const PokemonCatalogCard&,
                           const PokemonCatalogCard&) = default;
};

struct PokemonSetCatalogPack {
    std::string setId;
    std::string setName;
    std::vector<PokemonCatalogCard> cards;

    friend bool operator==(const PokemonSetCatalogPack&,
                           const PokemonSetCatalogPack&) = default;
};

struct PokemonSetCatalog {
    std::vector<PokemonSetCatalogPack> packs;

    [[nodiscard]] const PokemonSetCatalogPack* findPack(
        std::string_view setId) const;

    [[nodiscard]] bool empty() const noexcept { return packs.empty(); }

    friend bool operator==(const PokemonSetCatalog&,
                           const PokemonSetCatalog&) = default;
};

void to_json(nlohmann::json& j, const PokemonCatalogCard& c);
void from_json(const nlohmann::json& j, PokemonCatalogCard& c);
void to_json(nlohmann::json& j, const PokemonSetCatalogPack& p);
void from_json(const nlohmann::json& j, PokemonSetCatalogPack& p);
void to_json(nlohmann::json& j, const PokemonSetCatalog& c);
void from_json(const nlohmann::json& j, PokemonSetCatalog& c);

}  // namespace ccm
