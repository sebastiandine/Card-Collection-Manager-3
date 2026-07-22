#include "ccm/domain/Configuration.hpp"

namespace ccm {

void to_json(nlohmann::json& j, const Configuration& c) {
    j = nlohmann::json{
        {"dataStorage", c.dataStorage},
        {"defaultGame", c.defaultGame},
        {"theme", c.theme},
    };
}

void from_json(const nlohmann::json& j, Configuration& c) {
    j.at("dataStorage").get_to(c.dataStorage);
    j.at("defaultGame").get_to(c.defaultGame);
    // JapanesePokemon was folded into Pokemon (West/Asia region). Coerce so
    // older config.json files keep a valid user-facing default game.
    if (c.defaultGame == Game::JapanesePokemon) {
        c.defaultGame = Game::Pokemon;
    }
    c.theme = j.value("theme", Theme::Light);
}

}  // namespace ccm
