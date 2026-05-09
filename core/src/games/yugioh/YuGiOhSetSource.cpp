#include "ccm/games/yugioh/YuGiOhSetSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace ccm {

YuGiOhSetSource::YuGiOhSetSource(IHttpClient& http) : http_(http) {}

Result<std::vector<Set>> YuGiOhSetSource::parseResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_array()) {
            return Result<std::vector<Set>>::err(
                "YGOPRODeck response is not an array.");
        }
        std::vector<Set> out;
        out.reserve(j.size());
        for (const auto& entry : j) {
            Set s;
            s.id          = entry.value("set_code", "");
            s.name        = entry.value("set_name", "");
            std::string release = entry.value("tcg_date", "");
            for (char& ch : release) {
                if (ch == '-') ch = '/';
            }
            s.releaseDate = std::move(release);
            out.push_back(std::move(s));
        }
        std::sort(out.begin(), out.end(),
                  [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
        return Result<std::vector<Set>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(
            std::string("YGOPRODeck set parse error: ") + e.what());
    }
}

Result<std::vector<Set>> YuGiOhSetSource::fetchAll() {
    auto resp = http_.get(kEndpoint);
    if (!resp) return Result<std::vector<Set>>::err(resp.error());
    return parseResponse(resp.value());
}

}  // namespace ccm
