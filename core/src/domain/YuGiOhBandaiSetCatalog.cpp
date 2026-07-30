#include "ccm/domain/YuGiOhBandaiSetCatalog.hpp"

namespace ccm {

const YuGiOhBandaiSetCatalogPack* YuGiOhBandaiSetCatalog::findPack(
    std::string_view setId) const {
    for (const auto& pack : packs) {
        if (pack.setId == setId) return &pack;
    }
    return nullptr;
}

void to_json(nlohmann::json& j, const YuGiOhBandaiCatalogCard& c) {
    j = nlohmann::json{{"setNo", c.setNo}, {"name", c.name}, {"rarity", c.rarity}};
}

void from_json(const nlohmann::json& j, YuGiOhBandaiCatalogCard& c) {
    j.at("setNo").get_to(c.setNo);
    j.at("name").get_to(c.name);
    if (j.contains("rarity")) {
        j.at("rarity").get_to(c.rarity);
    } else {
        c.rarity.clear();
    }
}

void to_json(nlohmann::json& j, const YuGiOhBandaiSetCatalogPack& p) {
    j = nlohmann::json{{"id", p.setId}, {"name", p.setName}, {"cards", p.cards}};
}

void from_json(const nlohmann::json& j, YuGiOhBandaiSetCatalogPack& p) {
    j.at("id").get_to(p.setId);
    j.at("name").get_to(p.setName);
    j.at("cards").get_to(p.cards);
}

void to_json(nlohmann::json& j, const YuGiOhBandaiSetCatalog& c) {
    j = nlohmann::json{{"packs", c.packs}};
}

void from_json(const nlohmann::json& j, YuGiOhBandaiSetCatalog& c) {
    j.at("packs").get_to(c.packs);
}

}  // namespace ccm
