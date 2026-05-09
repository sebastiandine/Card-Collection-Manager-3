#include "ccm/domain/MagicCard.hpp"

namespace ccm {

void to_json(nlohmann::json& j, const MagicCard& c) {
    j = nlohmann::json{
        {"id",        c.id},
        {"amount",    c.amount},
        {"name",      c.name},
        {"set",       c.set},
        {"note",      c.note},
        {"images",    c.images},
        {"language",  c.language},
        {"condition", c.condition},
        {"foil",      c.foil},
        {"signed",    c.signed_},
        {"altered",   c.altered},
    };
}

void from_json(const nlohmann::json& j, MagicCard& c) {
    j.at("id").get_to(c.id);
    j.at("amount").get_to(c.amount);
    j.at("name").get_to(c.name);
    j.at("set").get_to(c.set);
    j.at("note").get_to(c.note);
    j.at("images").get_to(c.images);
    j.at("language").get_to(c.language);
    j.at("condition").get_to(c.condition);
    j.at("foil").get_to(c.foil);
    j.at("signed").get_to(c.signed_);
    j.at("altered").get_to(c.altered);
}

}  // namespace ccm
