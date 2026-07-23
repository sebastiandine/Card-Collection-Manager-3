#include "ccm/domain/DigiBattle99SetCatalog.hpp"

namespace ccm {

const DigiBattle99SetCatalogPack* DigiBattle99SetCatalog::findPack(
    std::string_view setId) const {
    for (const auto& pack : packs) {
        if (pack.setId == setId) return &pack;
    }
    return nullptr;
}

void to_json(nlohmann::json& j, const DigiBattle99CatalogCard& c) {
    j = nlohmann::json{{"setNo", c.setNo}, {"name", c.name}};
}

void from_json(const nlohmann::json& j, DigiBattle99CatalogCard& c) {
    j.at("setNo").get_to(c.setNo);
    j.at("name").get_to(c.name);
}

void to_json(nlohmann::json& j, const DigiBattle99SetCatalogPack& p) {
    j = nlohmann::json{{"id", p.setId}, {"name", p.setName}, {"cards", p.cards}};
}

void from_json(const nlohmann::json& j, DigiBattle99SetCatalogPack& p) {
    j.at("id").get_to(p.setId);
    j.at("name").get_to(p.setName);
    j.at("cards").get_to(p.cards);
}

void to_json(nlohmann::json& j, const DigiBattle99SetCatalog& c) {
    j = nlohmann::json{{"packs", c.packs}};
}

void from_json(const nlohmann::json& j, DigiBattle99SetCatalog& c) {
    j.at("packs").get_to(c.packs);
}

}  // namespace ccm
