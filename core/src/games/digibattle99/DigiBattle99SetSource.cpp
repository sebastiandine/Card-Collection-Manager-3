#include "ccm/games/digibattle99/DigiBattle99SetSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ccm {

namespace {

// Curated EN release dates for the vintage Digi-Battle product line.
// Series 1 Starter is verified 1999-06-01; other entries use digimoncard.io /
// checklist years (day unknown -> YYYY/01/01 or mid-year anchors for ordering).
const std::unordered_map<std::string, std::string>& curatedReleaseDates() {
    static const std::unordered_map<std::string, std::string> kDates{
        {"Series 1 Starter Set", "1999/06/01"},
        {"Series 1 Booster Pack", "1999/06/01"},
        {"Series 2 Booster Pack", "1999/09/01"},
        {"Series 3 Booster Pack", "2000/01/01"},
        {"Series 4 Booster Pack", "2000/06/01"},
        {"Series 5 Booster Pack", "2000/10/01"},
        {"Series 6 Booster Pack", "2001/01/01"},
        {"Street Starter Set 1", "2001/01/01"},
        {"Street Starter Set 2", "2001/02/01"},
        {"Street Starter Set 3", "2001/03/01"},
        {"Street Starter Set 4", "2001/04/01"},
        {"Digimon The Movie Promo Cards", "2000/10/01"},
    };
    return kDates;
}

std::string releaseDateForPack(const std::string& packName) {
    const auto& dates = curatedReleaseDates();
    const auto it = dates.find(packName);
    if (it != dates.end()) return it->second;
    return {};
}

}  // namespace

DigiBattle99SetSource::DigiBattle99SetSource(IHttpClient& http) : http_(http) {}

std::string DigiBattle99SetSource::slugifyPackName(std::string_view packName) {
    std::string out;
    out.reserve(packName.size());
    bool pendingHyphen = false;
    for (unsigned char ch : packName) {
        if (std::isalnum(ch)) {
            if (pendingHyphen && !out.empty()) out.push_back('-');
            pendingHyphen = false;
            out.push_back(static_cast<char>(std::tolower(ch)));
        } else {
            pendingHyphen = !out.empty();
        }
    }
    return out;
}

Result<std::vector<Set>> DigiBattle99SetSource::parseResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (j.is_object() && j.contains("error")) {
            return Result<std::vector<Set>>::err(
                j.value("error", std::string{"digimoncard.io set search error"}));
        }
        if (!j.is_array()) {
            return Result<std::vector<Set>>::err(
                "digimoncard.io Digi-Battle response is not a JSON array.");
        }

        // Preserve first-seen order of pack names, then sort by release date.
        std::unordered_set<std::string> seen;
        std::vector<std::string> packNames;
        packNames.reserve(16);
        for (const auto& entry : j) {
            if (!entry.contains("set_name") || !entry.at("set_name").is_array()) continue;
            for (const auto& pack : entry.at("set_name")) {
                if (!pack.is_string()) continue;
                const std::string name = pack.get<std::string>();
                if (name.empty()) continue;
                if (seen.insert(name).second) packNames.push_back(name);
            }
        }

        std::vector<Set> out;
        out.reserve(packNames.size());
        for (const auto& name : packNames) {
            Set s;
            s.id = slugifyPackName(name);
            s.name = name;
            s.releaseDate = releaseDateForPack(name);
            if (s.id.empty()) continue;
            out.push_back(std::move(s));
        }

        std::sort(out.begin(), out.end(), [](const Set& a, const Set& b) {
            if (a.releaseDate.empty() && !b.releaseDate.empty()) return false;
            if (!a.releaseDate.empty() && b.releaseDate.empty()) return true;
            if (a.releaseDate != b.releaseDate) return a.releaseDate < b.releaseDate;
            return a.name < b.name;
        });
        return Result<std::vector<Set>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(
            std::string("digimoncard.io Digi-Battle JSON parse error: ") + e.what());
    }
}

Result<std::vector<Set>> DigiBattle99SetSource::fetchAll() {
    auto resp = http_.get(kEndpoint);
    if (!resp) return Result<std::vector<Set>>::err(resp.error());
    return parseResponse(resp.value());
}

}  // namespace ccm
