#include "ccm/games/magic/MagicSetSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace ccm {

MagicSetSource::MagicSetSource(IHttpClient& http) : http_(http) {}

Result<std::vector<Set>> MagicSetSource::parseResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return Result<std::vector<Set>>::err("Scryfall response missing 'data' array.");
        }
        std::vector<Set> out;
        out.reserve(j.at("data").size());
        for (const auto& entry : j.at("data")) {
            // Filter out digital-only sets exactly like the Rust code.
            const bool digital = entry.value("digital", false);
            if (digital) continue;

            Set s;
            s.id   = entry.value("code", "");
            s.name = entry.value("name", "");
            // Scryfall returns "released_at" as YYYY-MM-DD; persisted data stores YYYY/MM/DD.
            std::string releasedAt = entry.value("released_at", "");
            std::replace(releasedAt.begin(), releasedAt.end(), '-', '/');
            s.releaseDate = std::move(releasedAt);
            out.push_back(std::move(s));
        }
        std::sort(out.begin(), out.end(),
                  [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
        return Result<std::vector<Set>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(std::string("Scryfall JSON parse error: ") + e.what());
    }
}

Result<std::vector<Set>> MagicSetSource::fetchAll() {
    auto resp = http_.get(kEndpoint);
    if (!resp) return Result<std::vector<Set>>::err(resp.error());
    return parseResponse(resp.value());
}

}  // namespace ccm
