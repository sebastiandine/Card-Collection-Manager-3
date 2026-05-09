#include "ccm/domain/Set.hpp"

namespace ccm {

void to_json(nlohmann::json& j, const Set& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"name", s.name},
        {"releaseDate", s.releaseDate},
    };
}

void from_json(const nlohmann::json& j, Set& s) {
    j.at("id").get_to(s.id);
    j.at("name").get_to(s.name);
    j.at("releaseDate").get_to(s.releaseDate);
}

}  // namespace ccm
