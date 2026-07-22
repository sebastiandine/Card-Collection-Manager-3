#include "ccm/games/pokemonjp/JapanesePokemonSetSource.hpp"

#include "ccm/util/Rfc3986.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>

namespace ccm {

namespace {

struct ClassicMissingProduct {
    const char* id;
    const char* nameEn;
    const char* releaseDate;  // YYYY/MM/DD
};

// Keep in sync with tools/pokemon_jp/classic_missing_sets.json and
// docs/assets-and-info-apis.md (Japanese Pokémon Info API).
constexpr std::array<ClassicMissingProduct, 11> kMissingClassicProducts{{
    // Day after Pokémon Jungle (PMCG2, 1997/03/05) so the set list places
    // Unnumbered Promo immediately after Jungle when sorted by releaseDate.
    {"UnnumberedPromo", "Unnumbered Promotional cards", "1997/03/06"},
    {"ExpSheet1", "Expansion Sheet Series 1", "1998/03/23"},
    {"NiviCG", "Nivi City Gym", "1998/04/26"},
    {"HanadaCG", "Hanada City Gym", "1998/04/26"},
    {"ExpSheet2", "Expansion Sheet Series 2", "1998/06/17"},
    {"KuchibaCG", "Kuchiba City Gym", "1998/07/25"},
    {"TamamushiCG", "Tamamushi City Gym", "1998/07/25"},
    {"ExpSheet3", "Expansion Sheet Series 3", "1998/11/24"},
    {"YamabukiCG", "Yamabuki City Gym", "1999/02/26"},
    {"GurenTG", "Guren Town Gym", "1999/02/26"},
    {"SouthernIslands", "Southern Islands", "1999/07/17"},
}};

const std::unordered_map<std::string, std::string>& setNameJaOverrides() {
    // Field-level corrections for known TCGdex JA mislabels (never edit cache).
    static const std::unordered_map<std::string, std::string> kOverrides{
        {"SV4a", "シャイニートレジャーex"},
    };
    return kOverrides;
}

[[nodiscard]] bool containsCjk(std::string_view s) noexcept {
    // Detect hiragana / katakana / CJK unified (UTF-8 lead bytes 0xE3–0xE9).
    // Do NOT treat Latin-1 accents (e.g. é in "Pokémon", lead 0xC3) as CJK —
    // that used to wipe catalog English names back to the set id.
    for (unsigned char ch : s) {
        if (ch >= 0xE3 && ch <= 0xE9) return true;
    }
    return false;
}

}  // namespace

JapanesePokemonSetSource::JapanesePokemonSetSource(
    IHttpClient& http, const JapanesePokemonEnCatalog& catalog)
    : http_(http), catalog_(catalog) {}

bool JapanesePokemonSetSource::shouldExcludeSetId(std::string_view setId) noexcept {
    // Chinese-region CS* entries are mislabeled on the JA endpoint.
    return setId.size() >= 2 && setId[0] == 'C' && setId[1] == 'S';
}

std::string JapanesePokemonSetSource::applySetNameOverride(std::string_view setId,
                                                           std::string nameJa) {
    const auto& overrides = setNameJaOverrides();
    const auto it = overrides.find(std::string(setId));
    if (it != overrides.end()) return it->second;
    return nameJa;
}

std::string JapanesePokemonSetSource::rewriteReleaseDate(std::string_view isoDate) {
    std::string out(isoDate);
    for (char& ch : out) {
        if (ch == '-') ch = '/';
    }
    return out;
}

std::string JapanesePokemonSetSource::buildSetDetailUrl(std::string_view setId) {
    return std::string("https://api.tcgdex.net/v2/ja/sets/") +
           rfc3986PercentEncode(setId);
}

Result<std::vector<Set>>
JapanesePokemonSetSource::parseListResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_array()) {
            return Result<std::vector<Set>>::err(
                "TCGdex JA sets response is not a JSON array.");
        }
        std::vector<Set> out;
        out.reserve(j.size());
        for (const auto& entry : j) {
            Set s;
            s.id = entry.value("id", "");
            if (s.id.empty() || shouldExcludeSetId(s.id)) continue;
            s.name = applySetNameOverride(s.id, entry.value("name", ""));
            s.releaseDate = {};  // filled from catalog or set detail
            out.push_back(std::move(s));
        }
        appendMissingClassicProducts(out);
        return Result<std::vector<Set>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(
            std::string("TCGdex JA sets JSON parse error: ") + e.what());
    }
}

void JapanesePokemonSetSource::appendMissingClassicProducts(std::vector<Set>& sets) {
    for (const auto& product : kMissingClassicProducts) {
        auto it = std::find_if(sets.begin(), sets.end(), [&](const Set& s) {
            return s.id == product.id;
        });
        if (it != sets.end()) {
            // Keep curated display name / sort date in sync (e.g. UnnumberedPromo
            // placement after Pokémon Jungle) even when the id was already cached.
            it->name = product.nameEn;
            it->releaseDate = product.releaseDate;
            continue;
        }
        Set s;
        s.id = product.id;
        s.name = product.nameEn;
        s.releaseDate = product.releaseDate;
        sets.push_back(std::move(s));
    }
}

Result<std::string>
JapanesePokemonSetSource::parseReleaseDate(const std::string& detailBody) {
    try {
        const auto j = nlohmann::json::parse(detailBody);
        if (!j.is_object()) {
            return Result<std::string>::err(
                "TCGdex JA set detail response is not a JSON object.");
        }
        const std::string raw = j.value("releaseDate", "");
        if (raw.empty()) {
            return Result<std::string>::ok(std::string{});
        }
        return Result<std::string>::ok(rewriteReleaseDate(raw));
    } catch (const std::exception& e) {
        return Result<std::string>::err(
            std::string("TCGdex JA set detail JSON parse error: ") + e.what());
    }
}

Result<std::vector<Set>> JapanesePokemonSetSource::fetchAll() {
    auto listResp = http_.get(kListEndpoint);
    if (!listResp) return Result<std::vector<Set>>::err(listResp.error());

    auto parsed = parseListResponse(listResp.value());
    if (!parsed) return parsed;

    std::vector<Set> out = std::move(parsed).value();
    for (auto& s : out) {
        // Prefer catalog English; never leave Japanese TCGdex names in Set.name
        // (the set picker must stay English-only).
        if (auto en = catalog_.findSet(s.id)) {
            if (!en->nameEn.empty()) s.name = en->nameEn;
            if (!en->releaseDate.empty()) s.releaseDate = en->releaseDate;
        }
        if (s.name.empty() || containsCjk(s.name)) {
            s.name = s.id;
        }
        if (!s.releaseDate.empty()) continue;

        auto detail = http_.get(buildSetDetailUrl(s.id));
        if (!detail) continue;  // keep set with empty date rather than fail all
        auto date = parseReleaseDate(detail.value());
        if (date && !date.value().empty()) {
            s.releaseDate = std::move(date).value();
        }
    }

    std::sort(out.begin(), out.end(),
              [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
    return Result<std::vector<Set>>::ok(std::move(out));
}

void JapanesePokemonSetSource::augmentCachedSets(std::vector<Set>& sets) const {
    // Stale caches may store set ids (or Japanese) as Set.name — re-apply the
    // bundled EN catalog so names like "Pokémon Jungle" are searchable again.
    for (auto& s : sets) {
        if (auto en = catalog_.findSet(s.id)) {
            if (!en->nameEn.empty()) s.name = en->nameEn;
            if (!en->releaseDate.empty() && s.releaseDate.empty()) {
                s.releaseDate = en->releaseDate;
            }
        }
        if (s.name.empty() || containsCjk(s.name)) {
            s.name = s.id;
        }
    }
    appendMissingClassicProducts(sets);
    std::sort(sets.begin(), sets.end(),
              [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
}

}  // namespace ccm
