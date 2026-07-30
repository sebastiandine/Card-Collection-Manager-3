#include "ccm/games/yugiohbandai/YuGiOhBandaiSetSource.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <regex>
#include <unordered_map>
#include <unordered_set>

namespace ccm {

namespace {

std::string trimCopy(std::string_view s) {
    while (!s.empty() &&
           (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' ||
            s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' ||
            s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

std::string urlEncode(std::string_view s) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

}  // namespace

YuGiOhBandaiSetSource::YuGiOhBandaiSetSource(IHttpClient& http) : http_(http) {}

const std::vector<YuGiOhBandaiSetSource::SetManifestEntry>&
YuGiOhBandaiSetSource::setManifest() {
    static const std::vector<SetManifestEntry> kManifest{
        {"ban1", "1st Generation", "1998/09/01",
         "Set Card Galleries:Yu-Gi-Oh! Bandai OCG: 1st Generation", ""},
        {"ban2", "2nd Generation", "1998/11/01",
         "Set Card Galleries:2nd Generation (Bandai)", ""},
        {"ban3", "3rd Generation", "1999/03/06",
         "Set Card Galleries:3rd Generation (Bandai)", ""},
        {"banpromo-j", "Jump Promos", "1998/01/01",
         "Set Card Galleries:Promotional Cards (Bandai)", "J"},
        {"banpromo-ta", "Toei Promos", "1999/03/06",
         "Set Card Galleries:Promotional Cards (Bandai)", "TA"},
        {"bansealdass", "Sealdass", "1999/06/01",
         "Set Card Galleries:Yu-Gi-Oh! Bandai Sealdass", ""},
    };
    return kManifest;
}

Result<std::vector<Set>> YuGiOhBandaiSetSource::parseResponse(
    const std::string& /*unused*/) {
    std::vector<Set> out;
    for (const auto& e : setManifest()) {
        out.push_back(Set{e.id, e.name, e.releaseDate});
    }
    return Result<std::vector<Set>>::ok(std::move(out));
}

Result<std::vector<Set>> YuGiOhBandaiSetSource::fetchAll() {
    return parseResponse({});
}

std::string YuGiOhBandaiSetSource::buildGalleryParseUrl(std::string_view pageTitle) {
    return std::string(
               "https://yugipedia.com/api.php?action=parse&format=json&formatversion=2"
               "&prop=wikitext&page=") +
           urlEncode(pageTitle);
}

std::string YuGiOhBandaiSetSource::normalizeCardNumber(std::string_view setNo) {
    std::string s = trimCopy(setNo);
    if (s.empty()) return {};

    // Strip a leading '#' if present.
    if (s.front() == '#') s.erase(s.begin());

    // Uppercase letter prefix forms: j1 / ta2.
    bool hasAlpha = false;
    for (char& c : s) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            hasAlpha = true;
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    if (hasAlpha) return s;

    // Pure decimal: strip leading zeros but keep a single zero.
    std::size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0') ++i;
    return s.substr(i);
}

std::string YuGiOhBandaiSetSource::expandRarityCode(std::string_view code) {
    const std::string c = trimCopy(code);
    if (c == "C") return "Common";
    if (c == "R") return "Rare";
    if (c == "SR") return "Super Rare";
    if (c == "UR") return "Ultra Rare";
    if (c == "HFR" || c == "Holo Seal" || c == "HS") return "Holo Seal";
    if (c.empty()) return {};
    return c;
}

std::string YuGiOhBandaiSetSource::englishNameFromGalleryTitle(
    std::string_view pageTitle) {
    std::string name = trimCopy(pageTitle);
    const auto stripSuffix = [&](std::string_view suffix) {
        if (name.size() > suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            name.resize(name.size() - suffix.size());
            name = trimCopy(name);
        }
    };
    stripSuffix(" (Bandai Sealdass)");
    stripSuffix(" (English Bandai)");
    stripSuffix(" (Bandai)");
    return name;
}

std::string YuGiOhBandaiSetSource::setIdForNumber(std::string_view setNo) {
    const std::string n = normalizeCardNumber(setNo);
    if (n.empty()) return {};
    if (!n.empty() && (n[0] == 'J' || n[0] == 'j')) return "banpromo-j";
    if (n.size() >= 2 && (n[0] == 'T' || n[0] == 't') &&
        (n[1] == 'A' || n[1] == 'a')) {
        return "banpromo-ta";
    }

    // Pure decimal → generation by range. Callers that need Sealdass must
    // pass set context; number alone cannot disambiguate 1–42 vs Sealdass.
    bool pureDecimal = true;
    for (char c : n) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            pureDecimal = false;
            break;
        }
    }
    if (!pureDecimal) return {};

    const int v = std::stoi(n);
    if (v >= 1 && v <= 42) return "ban1";
    if (v >= 43 && v <= 88) return "ban2";
    if (v >= 89 && v <= 118) return "ban3";
    return {};
}

std::string YuGiOhBandaiSetSource::setNameForId(std::string_view setId) {
    for (const auto& e : setManifest()) {
        if (e.id == setId) return e.name;
    }
    return {};
}

Result<std::vector<YuGiOhBandaiCatalogCard>>
YuGiOhBandaiSetSource::parseGalleryWikitext(const std::string& wikitext) {
    using R = Result<std::vector<YuGiOhBandaiCatalogCard>>;
    std::vector<YuGiOhBandaiCatalogCard> out;

    // Generation galleries (raw):
    //   … | {{pound}}014 ([[R]]) {{Gallery card names|Dark Magician (Bandai)|ja}}
    // Promo galleries (often expanded with <br />):
    //   … | [[TA2]] ([[SR]])<br />{{Gallery card names|Blue-Eyes White Dragon's 3-Body Connection|ja}}
    static const std::regex kLine(
        R"((?:\{\{pound\}\}|\[\[)([A-Za-z0-9]+)(?:\]\])?(?:\s*\(\[\[([A-Za-z0-9]+)\]\]\))?[^\n]*?\{\{Gallery card names\|([^}|]+))",
        std::regex::ECMAScript);

    std::unordered_set<std::string> seen;
    for (std::sregex_iterator it(wikitext.begin(), wikitext.end(), kLine), end;
         it != end; ++it) {
        const std::smatch& m = *it;
        YuGiOhBandaiCatalogCard card;
        card.setNo = normalizeCardNumber(m[1].str());
        if (card.setNo.empty()) continue;
        if (m[2].matched) {
            card.rarity = expandRarityCode(m[2].str());
        }
        card.name = englishNameFromGalleryTitle(m[3].str());
        if (card.name.empty()) continue;
        if (!seen.insert(card.setNo).second) continue;
        out.push_back(std::move(card));
    }

    return R::ok(std::move(out));
}

Result<YuGiOhBandaiSetSource::FetchWithCatalog>
YuGiOhBandaiSetSource::fetchAllWithCatalog() {
    using R = Result<FetchWithCatalog>;

    auto sets = parseResponse({});
    if (!sets) return R::err(sets.error());

    YuGiOhBandaiSetCatalog catalog;
    std::unordered_map<std::string, std::string> pageCache;

    for (const auto& entry : setManifest()) {
        const std::string page = entry.galleryPage;
        std::string body;
        auto cached = pageCache.find(page);
        if (cached != pageCache.end()) {
            body = cached->second;
        } else {
            const std::string url = buildGalleryParseUrl(page);
            auto resp = http_.get(url);
            if (!resp) return R::err(resp.error());
            body = std::move(resp).value();
            pageCache.emplace(page, body);
        }

        std::string wikitext;
        try {
            const auto j = nlohmann::json::parse(body);
            if (!j.contains("parse") || !j.at("parse").contains("wikitext")) {
                return R::err("Yugipedia gallery response missing parse.wikitext");
            }
            wikitext = j.at("parse").at("wikitext").get<std::string>();
        } catch (const std::exception& e) {
            return R::err(std::string("Yugipedia gallery JSON parse error: ") +
                          e.what());
        }

        auto cards = parseGalleryWikitext(wikitext);
        if (!cards) return R::err(cards.error());

        YuGiOhBandaiSetCatalogPack pack;
        pack.setId = entry.id;
        pack.setName = entry.name;
        const std::string prefix = entry.setNoPrefix;
        for (const auto& card : cards.value()) {
            if (!prefix.empty()) {
                if (card.setNo.size() < prefix.size() ||
                    card.setNo.compare(0, prefix.size(), prefix) != 0) {
                    continue;
                }
            }
            pack.cards.push_back(card);
        }

        catalog.packs.push_back(std::move(pack));
    }

    FetchWithCatalog out;
    out.sets = std::move(sets).value();
    out.catalog = std::move(catalog);
    return R::ok(std::move(out));
}

}  // namespace ccm
