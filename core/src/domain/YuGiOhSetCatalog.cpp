#include "ccm/domain/YuGiOhSetCatalog.hpp"

namespace ccm {

const YuGiOhSetCatalogPack* YuGiOhSetCatalog::findPack(std::string_view setId) const {
    for (const auto& pack : packs) {
        if (pack.setId == setId) return &pack;
    }
    return nullptr;
}

void to_json(nlohmann::json& j, const YuGiOhCatalogCard& c) {
    j = nlohmann::json{{"setNo", c.setNo}, {"name", c.name}};
    if (!c.rarity.empty()) j["rarity"] = c.rarity;
}

void from_json(const nlohmann::json& j, YuGiOhCatalogCard& c) {
    j.at("setNo").get_to(c.setNo);
    j.at("name").get_to(c.name);
    c.rarity = j.value("rarity", "");
}

void to_json(nlohmann::json& j, const YuGiOhSetCatalogPack& p) {
    j = nlohmann::json{{"id", p.setId}, {"name", p.setName}, {"cards", p.cards}};
}

void from_json(const nlohmann::json& j, YuGiOhSetCatalogPack& p) {
    j.at("id").get_to(p.setId);
    j.at("name").get_to(p.setName);
    j.at("cards").get_to(p.cards);
}

void to_json(nlohmann::json& j, const YuGiOhSetCatalog& c) {
    j = nlohmann::json{{"packs", c.packs}};
}

void from_json(const nlohmann::json& j, YuGiOhSetCatalog& c) {
    j.at("packs").get_to(c.packs);
}

}  // namespace ccm
