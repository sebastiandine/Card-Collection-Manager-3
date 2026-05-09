#include "ccm/games/pokemon/PokemonSetSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace ccm {

PokemonSetSource::PokemonSetSource(IHttpClient& http) : http_(http) {}

Result<std::vector<Set>> PokemonSetSource::parseResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return Result<std::vector<Set>>::err(
                "Pokemon TCG API response missing 'data' array.");
        }
        std::vector<Set> out;
        out.reserve(j.at("data").size());
        for (const auto& entry : j.at("data")) {
            Set s;
            s.id          = entry.value("id", "");
            s.name        = entry.value("name", "");
            // Pokemon TCG API already returns "releaseDate" in YYYY/MM/DD;
            // no separator rewrite needed (cf. Scryfall's "released_at").
            s.releaseDate = entry.value("releaseDate", "");
            out.push_back(std::move(s));
        }
        std::sort(out.begin(), out.end(),
                  [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
        return Result<std::vector<Set>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(
            std::string("Pokemon TCG JSON parse error: ") + e.what());
    }
}

Result<std::vector<Set>> PokemonSetSource::fetchAll() {
    auto resp = http_.get(kEndpoint);
    if (!resp) return Result<std::vector<Set>>::err(resp.error());
    return parseResponse(resp.value());
}

}  // namespace ccm
