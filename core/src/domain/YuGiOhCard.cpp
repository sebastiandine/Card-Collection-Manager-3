#include "ccm/domain/YuGiOhCard.hpp"

namespace ccm {

void to_json(nlohmann::json& j, const YuGiOhCard& c) {
    j = nlohmann::json{
        {"id",           c.id},
        {"amount",       c.amount},
        {"name",         c.name},
        {"set",          c.set},
        {"setNo",        c.setNo},
        {"note",         c.note},
        {"images",       c.images},
        {"language",     c.language},
        {"condition",    c.condition},
        {"firstEdition", c.firstEdition},
        {"rarity",       c.rarity},
        {"rarityCode",   c.rarityCode},
        {"signed",       c.signed_},
        {"altered",      c.altered},
    };
}

void from_json(const nlohmann::json& j, YuGiOhCard& c) {
    j.at("id").get_to(c.id);
    j.at("amount").get_to(c.amount);
    j.at("name").get_to(c.name);
    j.at("set").get_to(c.set);
    j.at("setNo").get_to(c.setNo);
    j.at("note").get_to(c.note);
    j.at("images").get_to(c.images);
    j.at("language").get_to(c.language);
    j.at("condition").get_to(c.condition);
    j.at("firstEdition").get_to(c.firstEdition);
    j.at("rarity").get_to(c.rarity);
    if (j.contains("rarityCode")) j.at("rarityCode").get_to(c.rarityCode);
    else c.rarityCode.clear();
    j.at("signed").get_to(c.signed_);
    j.at("altered").get_to(c.altered);
}

}  // namespace ccm
