#include "ccm/games/yugioh/YuGiOhSetSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace ccm {
namespace {

struct YuGiOhSetAlias {
    const char* code;
    const char* name;
    const char* releaseDate;
};

constexpr std::array<YuGiOhSetAlias, 6> kMissing25thAnniversaryReprints{{
    // Keep this list in sync with docs/assets-and-info-apis.md (Info API section).
    {"LOB-25TH", "Legend of Blue Eyes White Dragon (25th Anniversary Edition)", "2023/04/20"},
    {"MRD-25TH", "Metal Raiders (25th Anniversary Edition)", "2023/04/20"},
    {"SRL-25TH", "Spell Ruler (25th Anniversary Edition)", "2023/04/20"},
    {"PSV-25TH", "Pharaoh's Servant (25th Anniversary Edition)", "2023/04/20"},
    {"DCR-25TH", "Dark Crisis (25th Anniversary Edition)", "2023/04/20"},
    {"IOC-25TH", "Invasion of Chaos (25th Anniversary Edition)", "2023/06/08"},
}};

void appendMissingSetAliases(std::vector<Set>& sets) {
    for (const auto& alias : kMissing25thAnniversaryReprints) {
        const bool exists = std::any_of(
            sets.begin(), sets.end(), [&](const Set& s) { return s.name == alias.name; });
        if (exists) continue;

        Set s;
        s.id = alias.code;
        s.name = alias.name;
        s.releaseDate = alias.releaseDate;
        sets.push_back(std::move(s));
    }
}

}  // namespace

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
        appendMissingSetAliases(out);
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
