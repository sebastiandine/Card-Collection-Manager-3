#pragma once

// Configuration: matches the persisted `config.json` schema used by this app.
//
//   { "dataStorage": "/abs/path", "defaultGame": "Magic" }

#include "ccm/domain/Enums.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace ccm {

struct Configuration {
    std::string dataStorage;
    Game        defaultGame{Game::Magic};
    Theme       theme{Theme::Light};

    friend bool operator==(const Configuration&, const Configuration&) = default;
};

void to_json(nlohmann::json& j, const Configuration& c);
void from_json(const nlohmann::json& j, Configuration& c);

}  // namespace ccm
