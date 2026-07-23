#include "ccm/games/pokemonjp/JapanesePokemonSetSource.hpp"

#include "ccm/games/pokemonjp/JapanesePokemonCardPreviewSource.hpp"
#include "ccm/util/Rfc3986.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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

void gapFillFromEnCatalog(PokemonSetCatalogPack& pack,
                          const JapanesePokemonEnCatalog& enCatalog) {
    std::unordered_set<std::string> seen;
    for (const auto& card : pack.cards) {
        seen.insert(JapanesePokemonCardPreviewSource::normalizeLocalId(card.setNo));
    }
    for (const auto& print : enCatalog.printsForSet(pack.setId)) {
        const std::string localId =
            JapanesePokemonCardPreviewSource::normalizeLocalId(print.localId);
        if (localId.empty() || !seen.insert(localId).second) continue;
        std::string name = print.nameEn;
        if (name.empty()) name = print.nameJa;
        if (name.empty()) name = localId;
        pack.cards.push_back(PokemonCatalogCard{localId, std::move(name)});
    }
}

void sortPackCards(PokemonSetCatalogPack& pack) {
    std::sort(pack.cards.begin(), pack.cards.end(),
              [](const PokemonCatalogCard& a, const PokemonCatalogCard& b) {
                  if (a.setNo != b.setNo) return a.setNo < b.setNo;
                  return a.name < b.name;
              });
}

void applyEnglishSetName(Set& s, const JapanesePokemonEnCatalog& catalog) {
    if (auto en = catalog.findSet(s.id)) {
        if (!en->nameEn.empty()) s.name = en->nameEn;
        if (!en->releaseDate.empty()) s.releaseDate = en->releaseDate;
    }
    if (s.name.empty() || containsCjk(s.name)) {
        s.name = s.id;
    }
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

PokemonSetCatalogPack JapanesePokemonSetSource::catalogPackFromEnCatalog(
    const Set& set, const JapanesePokemonEnCatalog& enCatalog) {
    PokemonSetCatalogPack pack;
    pack.setId = set.id;
    pack.setName = set.name.empty() ? set.id : set.name;
    for (const auto& print : enCatalog.printsForSet(set.id)) {
        const std::string localId =
            JapanesePokemonCardPreviewSource::normalizeLocalId(print.localId);
        if (localId.empty()) continue;
        std::string name = print.nameEn;
        if (name.empty()) name = print.nameJa;
        if (name.empty()) name = localId;
        pack.cards.push_back(PokemonCatalogCard{localId, std::move(name)});
    }
    sortPackCards(pack);
    return pack;
}

Result<PokemonSetCatalogPack> JapanesePokemonSetSource::parseCatalogPackFromSetDetail(
    const std::string& detailBody,
    const Set&         set,
    const JapanesePokemonEnCatalog& enCatalog) {
    auto rows = JapanesePokemonCardPreviewSource::parseSetCards(detailBody);
    if (!rows) {
        // Transient/NotFound from parse — treat empty cards as catalog-only.
        if (rows.error().kind == PreviewLookupError::Kind::NotFound) {
            auto pack = catalogPackFromEnCatalog(set, enCatalog);
            if (pack.cards.empty()) {
                return Result<PokemonSetCatalogPack>::err(
                    "No cards for set " + set.id);
            }
            return Result<PokemonSetCatalogPack>::ok(std::move(pack));
        }
        return Result<PokemonSetCatalogPack>::err(rows.error().message);
    }

    PokemonSetCatalogPack pack;
    pack.setId = set.id;
    pack.setName = set.name.empty() ? set.id : set.name;

    std::unordered_set<std::string> seen;
    for (const auto& row : rows.value()) {
        const std::string localId =
            JapanesePokemonCardPreviewSource::normalizeLocalId(row.localId);
        if (localId.empty() || !seen.insert(localId).second) continue;

        std::string name;
        if (auto print = enCatalog.findPrint(set.id, localId)) {
            name = print->nameEn;
            if (name.empty()) name = print->nameJa;
        }
        if (name.empty()) name = row.nameJa;
        if (name.empty()) name = localId;
        pack.cards.push_back(PokemonCatalogCard{localId, std::move(name)});
    }

    gapFillFromEnCatalog(pack, enCatalog);
    sortPackCards(pack);
    if (pack.cards.empty()) {
        return Result<PokemonSetCatalogPack>::err("No cards for set " + set.id);
    }
    return Result<PokemonSetCatalogPack>::ok(std::move(pack));
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
        applyEnglishSetName(s, catalog_);
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

Result<JapanesePokemonSetSource::FetchWithCatalog>
JapanesePokemonSetSource::fetchAllWithCatalog() {
    auto listResp = http_.get(kListEndpoint);
    if (!listResp) return Result<FetchWithCatalog>::err(listResp.error());

    auto parsed = parseListResponse(listResp.value());
    if (!parsed) return Result<FetchWithCatalog>::err(parsed.error());

    std::vector<Set> sets = std::move(parsed).value();
    PokemonSetCatalog catalog;
    catalog.packs.reserve(sets.size());

    for (auto& s : sets) {
        applyEnglishSetName(s, catalog_);

        auto detail = http_.get(buildSetDetailUrl(s.id));
        if (!detail) {
            // Classic / catalog-only products often have no TCGdex detail.
            auto pack = catalogPackFromEnCatalog(s, catalog_);
            if (!pack.cards.empty()) {
                catalog.packs.push_back(std::move(pack));
            }
            continue;
        }

        if (s.releaseDate.empty()) {
            auto date = parseReleaseDate(detail.value());
            if (date && !date.value().empty()) {
                s.releaseDate = std::move(date).value();
            }
        }

        auto pack = parseCatalogPackFromSetDetail(detail.value(), s, catalog_);
        if (pack) {
            catalog.packs.push_back(std::move(pack).value());
        } else {
            auto fallback = catalogPackFromEnCatalog(s, catalog_);
            if (!fallback.cards.empty()) {
                catalog.packs.push_back(std::move(fallback));
            }
        }
    }

    std::sort(sets.begin(), sets.end(),
              [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
    std::sort(catalog.packs.begin(), catalog.packs.end(),
              [](const PokemonSetCatalogPack& a, const PokemonSetCatalogPack& b) {
                  return a.setName < b.setName;
              });

    FetchWithCatalog out;
    out.sets = std::move(sets);
    out.catalog = std::move(catalog);
    return Result<FetchWithCatalog>::ok(std::move(out));
}

void JapanesePokemonSetSource::augmentCachedSets(std::vector<Set>& sets) const {
    // Stale caches may store set ids (or Japanese) as Set.name — re-apply the
    // bundled EN catalog so names like "Pokémon Jungle" are searchable again.
    for (auto& s : sets) {
        applyEnglishSetName(s, catalog_);
    }
    appendMissingClassicProducts(sets);
    std::sort(sets.begin(), sets.end(),
              [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
}

}  // namespace ccm
