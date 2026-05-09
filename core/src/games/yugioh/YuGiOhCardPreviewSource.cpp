#include "ccm/games/yugioh/YuGiOhCardPreviewSource.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace ccm {

namespace {

// RFC 3986 percent-encoder. Same rules as the Magic implementation; private
// here so the YGO and Magic code paths can drift independently if the future
// requires it (Yugipedia's MediaWiki API is fine with %20 for spaces and %7C
// for the `|` separator inside `titles=`).
std::string urlEncode(std::string_view in) {
    std::ostringstream out;
    out.fill('0');
    out << std::hex << std::uppercase;
    for (unsigned char c : in) {
        const bool unreserved =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~';
        if (unreserved) {
            out << static_cast<char>(c);
        } else {
            out << '%';
            out.width(2);
            out << static_cast<unsigned int>(c);
        }
    }
    return out.str();
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string toLower(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

// Pull the standard art URL out of a YGOPRODeck card object. We deliberately
// always return card_images[0]: when no `cardset=` filter is applied, that
// slot is the original/standard artwork (alt-art passcodes follow), which is
// the closest fallback we have when Yugipedia has no scan for this printing.
std::string imageFromCard(const nlohmann::json& card) {
    if (!card.contains("card_images") || !card.at("card_images").is_array() || card.at("card_images").empty()) {
        return {};
    }
    const auto& first = card.at("card_images").at(0);
    if (first.contains("image_url") && first.at("image_url").is_string()) {
        return first.at("image_url").get<std::string>();
    }
    if (first.contains("image_url_small") && first.at("image_url_small").is_string()) {
        return first.at("image_url_small").get<std::string>();
    }
    if (first.contains("image_url_cropped") && first.at("image_url_cropped").is_string()) {
        return first.at("image_url_cropped").get<std::string>();
    }
    return {};
}

// Split a setNo encoded by the UI as `<setNo>||<rarity>||<edition>` into its
// three positional fields. Any missing trailing field becomes an empty
// string, so older callers that pass just `<setNo>` keep working.
struct ParsedSetNo {
    std::string setNo;
    std::string rarity;
    std::string edition;  // "1E" / "UE" / "" (unknown)
};
ParsedSetNo parseSetNoTuple(std::string_view raw) {
    std::string s(raw);
    ParsedSetNo p;
    const auto a = s.find("||");
    if (a == std::string::npos) {
        p.setNo = trim(std::move(s));
        return p;
    }
    p.setNo = trim(s.substr(0, a));
    std::string rest = s.substr(a + 2);
    const auto b = rest.find("||");
    if (b == std::string::npos) {
        p.rarity = trim(std::move(rest));
        return p;
    }
    p.rarity = trim(rest.substr(0, b));
    p.edition = trim(rest.substr(b + 2));
    return p;
}

}  // namespace

YuGiOhCardPreviewSource::YuGiOhCardPreviewSource(IHttpClient& http) : http_(http) {}

// ============================================================================
// Yugipedia (image-preview path)
// ============================================================================

std::string YuGiOhCardPreviewSource::normalizeName(std::string_view name) {
    // Yugipedia's image policy strips whitespace and a fixed set of
    // punctuation from the displayed card name to produce the file slug.
    // Reference: https://yugipedia.com/wiki/Yugipedia:Image_policy
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (c <= 0x20) continue;  // whitespace, including non-breaking
        switch (c) {
            case '#': case ',': case '.': case ':': case '\'': case '"':
            case '?': case '!': case '&': case '@': case '%': case '=':
            case '[': case ']': case '<': case '>': case '/': case '\\':
            case '-': case '*': case ';': case '`':
                continue;
            default:
                break;
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

std::string YuGiOhCardPreviewSource::rarityCodeFor(std::string_view rarityName) {
    // Compare case-insensitively, ignoring whitespace, against a table of
    // CCM3 dialog values (see ui_wx/src/YuGiOhCardEditDialog.cpp:kRarityOptions)
    // plus a few extras occasionally seen in imported collections. The codes
    // are the ones Yugipedia uses in image filenames.
    std::string lc;
    lc.reserve(rarityName.size());
    for (unsigned char c : rarityName) {
        if (std::isspace(c)) continue;
        lc.push_back(static_cast<char>(std::tolower(c)));
    }
    static const std::array<std::pair<std::string_view, std::string_view>, 32> kTable = {{
        {"common",                       "C"},
        {"shortprint",                   "SP"},
        {"supershortprint",              "SSP"},
        {"normalrare",                   "NR"},
        {"rare",                         "R"},
        {"superrare",                    "SR"},
        {"ultrarare",                    "UR"},
        {"ultimaterare",                 "UtR"},
        {"secretrare",                   "ScR"},
        {"prismaticsecretrare",          "PScR"},
        {"extrasecretrare",              "EScR"},
        {"ultrasecretrare",              "UScR"},
        {"platinumsecretrare",           "PtScR"},
        {"goldsecretrare",               "GScR"},
        {"ghostrare",                    "GR"},
        {"goldrare",                     "GUR"},
        {"premiumgoldrare",              "PGR"},
        {"goldenrare",                   "GUR"},
        {"starfoilrare",                 "SFR"},
        {"shatterfoilrare",              "SHR"},
        {"mosaicrare",                   "MSR"},
        {"parallelrare",                 "PR"},
        {"superparallelrare",            "SPR"},
        {"ultraparallelrare",            "UPR"},
        {"holographicrare",              "HGR"},
        {"starlightrare",                "StR"},
        {"collectorsrare",               "ColR"},
        {"prismaticcollectorsrare",      "PColR"},
        {"quartercenturysecretrare",     "QCScR"},
        {"prismaticultimaterare",        "PUtR"},
        {"prismaticredsecretrare",       "PRScR"},
        {"silverletter",                 "SLR"},
    }};
    for (const auto& [k, v] : kTable) {
        if (lc == k) return std::string(v);
    }
    return {};
}

std::string YuGiOhCardPreviewSource::extractSetCode(std::string_view setNo) {
    std::string s(setNo);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    const auto dash = s.find('-');
    if (dash == std::string::npos) return s;
    return s.substr(0, dash);
}

std::vector<std::string> YuGiOhCardPreviewSource::buildCandidateFilenames(
    std::string_view name,
    std::string_view setCode,
    std::string_view rarityCode,
    bool firstEdition) {
    std::vector<std::string> out;
    const std::string slug = normalizeName(name);
    if (slug.empty() || setCode.empty()) return out;

    // English-only region candidates, in rough usage order: EN is the
    // current default, NA was used on most LOB-era prints, EU/AU show up
    // sporadically. Always English regardless of the card's stored Language.
    static constexpr std::array<std::string_view, 4> kRegions =
        {"EN", "NA", "EU", "AU"};

    // Edition candidates: prefer the printed edition the user has, then
    // try the opposite, then fall back to LE for promo-type prints.
    std::array<std::string_view, 3> editions = {"", "", "LE"};
    if (firstEdition) {
        editions[0] = "1E";
        editions[1] = "UE";
    } else {
        editions[0] = "UE";
        editions[1] = "1E";
    }

    // Two extension variants: Yugipedia has a mix of .png (modern) and .jpg
    // (older uploads) for the same era. Both are common for LOB-era cards.
    static constexpr std::array<std::string_view, 2> kExts = {"png", "jpg"};

    auto pushCombos = [&](std::string_view rarity) {
        for (auto edition : editions) {
            for (auto region : kRegions) {
                for (auto ext : kExts) {
                    std::string fn;
                    fn.reserve(slug.size() + setCode.size() + 16);
                    fn += slug;
                    fn += '-'; fn.append(setCode);
                    fn += '-'; fn.append(region);
                    if (!rarity.empty()) {
                        fn += '-'; fn.append(rarity);
                    }
                    fn += '-'; fn.append(edition);
                    fn += '.'; fn.append(ext);
                    out.push_back(std::move(fn));
                }
            }
        }
    };

    // Primary attempts include the rarity slot. If we don't know the rarity
    // we skip straight to the rarity-less fallback (some sets are uniform
    // rarity and the upload omits the slot).
    if (!rarityCode.empty()) {
        pushCombos(rarityCode);
    }
    pushCombos("");
    return out;
}

std::string YuGiOhCardPreviewSource::buildYugipediaQueryUrl(
    const std::vector<std::string>& filenames) {
    // MediaWiki batch query: `titles=File:A|File:B|File:C` (URL-encoded).
    // One HTTP call returns imageinfo for every page whose file exists; the
    // missing ones come back tagged with `"missing": ""`.
    std::string joined;
    for (size_t i = 0; i < filenames.size(); ++i) {
        if (i > 0) joined += "|";
        joined += "File:";
        joined += filenames[i];
    }
    std::string url =
        "https://yugipedia.com/api.php?action=query&format=json"
        "&prop=imageinfo&iiprop=url&titles=";
    url += urlEncode(joined);
    return url;
}

Result<std::string, PreviewLookupError> YuGiOhCardPreviewSource::parseYugipediaResponse(
    const std::string& body,
    const std::vector<std::string>& filenameOrder) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("query") || !j.at("query").is_object()) {
            return R::err({K::Transient, "Yugipedia response missing 'query' object."});
        }
        const auto& pages = j.at("query").value("pages", nlohmann::json::object());
        if (!pages.is_object()) {
            return R::err({K::Transient, "Yugipedia response missing 'query.pages'."});
        }

        // Build a name->URL map. MediaWiki returns the title with namespace
        // ("File:...") and may have replaced spaces with underscores; our
        // candidate filenames never contain spaces, so a direct compare on
        // the bit after "File:" is sufficient.
        std::unordered_map<std::string, std::string> resolved;
        resolved.reserve(filenameOrder.size());
        for (auto it = pages.begin(); it != pages.end(); ++it) {
            const auto& page = it.value();
            if (!page.contains("imageinfo")) continue;
            const auto& info = page.at("imageinfo");
            if (!info.is_array() || info.empty()) continue;
            const auto& info0 = info.at(0);
            if (!info0.contains("url") || !info0.at("url").is_string()) continue;

            std::string title = page.value("title", "");
            constexpr std::string_view kPrefix = "File:";
            if (title.rfind(kPrefix, 0) == 0) title.erase(0, kPrefix.size());
            resolved[title] = info0.at("url").get<std::string>();
        }

        // Walk our ordered candidate list and return the first hit. This is
        // how priority works: 1E English first, then UE, then jpg, etc.
        for (const auto& fn : filenameOrder) {
            auto it = resolved.find(fn);
            if (it != resolved.end() && !it->second.empty()) {
                return R::ok(it->second);
            }
        }
        // Every candidate was tagged "missing" => Yugipedia confirmed there
        // is no English scan for this printing. Treat as NotFound; the
        // YGOPRODeck fallback may still surface a generic art.
        return R::err({K::NotFound, "No matching Yugipedia scan found."});
    } catch (const std::exception& e) {
        return R::err({K::Transient,
            std::string("Yugipedia JSON parse error: ") + e.what()});
    }
}

// ============================================================================
// YGOPRODeck (auto-detect path + last-resort fallback)
// ============================================================================

std::string YuGiOhCardPreviewSource::buildSearchUrl(std::string_view name,
                                                    std::string_view setName) {
    std::string url =
        std::string("https://db.ygoprodeck.com/api/v7/cardinfo.php?fname=") + urlEncode(name);
    if (!setName.empty()) {
        url += "&cardset=";
        url += urlEncode(setName);
    }
    return url;
}

Result<std::string, PreviewLookupError> YuGiOhCardPreviewSource::parseFallbackImageUrl(
    const std::string& body, std::string_view name) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return R::err({K::Transient, "YGOPRODeck response missing 'data' array."});
        }
        const auto& data = j.at("data");
        if (data.empty()) {
            return R::err({K::NotFound, "YGOPRODeck returned no matching cards."});
        }
        const std::string wantedNameLower = toLower(trim(std::string(name)));

        // Prefer the exact-name match: the fuzzy `fname=` search can mix in
        // sibling cards (Dark Magician + Dark Magician Girl), and we don't
        // want to land on a sibling's standard art.
        for (const auto& card : data) {
            const std::string cardName = trim(card.value("name", ""));
            if (!wantedNameLower.empty() && toLower(cardName) == wantedNameLower) {
                const std::string image = imageFromCard(card);
                if (!image.empty()) return R::ok(image);
            }
        }
        // Failing that, take whatever YGOPRODeck ranked first.
        const std::string image = imageFromCard(data.at(0));
        if (!image.empty()) {
            return R::ok(image);
        }
        return R::err({K::NotFound, "Card has no image variants."});
    } catch (const std::exception& e) {
        return R::err({K::Transient,
            std::string("YGOPRODeck JSON parse error: ") + e.what()});
    }
}

Result<AutoDetectedPrint> YuGiOhCardPreviewSource::parseFirstPrint(
    const std::string& body, std::string_view preferredSetName) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array() || j.at("data").empty()) {
            return Result<AutoDetectedPrint>::err("YGOPRODeck returned no matching cards.");
        }
        const std::string wantedSet = trim(std::string(preferredSetName));
        for (const auto& card : j.at("data")) {
            if (!card.contains("card_sets") || !card.at("card_sets").is_array()) continue;
            for (const auto& print : card.at("card_sets")) {
                const std::string setName = trim(print.value("set_name", ""));
                if (!wantedSet.empty() && setName != wantedSet) continue;
                AutoDetectedPrint out;
                out.setNo = trim(print.value("set_code", ""));
                out.rarity = trim(print.value("set_rarity", ""));
                if (!out.setNo.empty() || !out.rarity.empty()) {
                    return Result<AutoDetectedPrint>::ok(std::move(out));
                }
            }
        }
        // Fallback: no preferred-set print found; pick first available print.
        const auto& firstCard = j.at("data").at(0);
        if (firstCard.contains("card_sets") && firstCard.at("card_sets").is_array()
            && !firstCard.at("card_sets").empty()) {
            const auto& print = firstCard.at("card_sets").at(0);
            AutoDetectedPrint out;
            out.setNo = trim(print.value("set_code", ""));
            out.rarity = trim(print.value("set_rarity", ""));
            if (!out.setNo.empty() || !out.rarity.empty()) {
                return Result<AutoDetectedPrint>::ok(std::move(out));
            }
        }
        return Result<AutoDetectedPrint>::err("Could not auto-detect set print metadata.");
    } catch (const std::exception& e) {
        return Result<AutoDetectedPrint>::err(
            std::string("YGOPRODeck JSON parse error: ") + e.what());
    }
}

// ============================================================================
// Public ICardPreviewSource API
// ============================================================================

Result<std::string, PreviewLookupError>
YuGiOhCardPreviewSource::fetchImageUrl(std::string_view name,
                                       std::string_view /*setId*/,
                                       std::string_view setNo) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    const ParsedSetNo p = parseSetNoTuple(setNo);
    const std::string setCode = extractSetCode(p.setNo);
    const std::string rarityCode = rarityCodeFor(p.rarity);
    const bool firstEdition = (p.edition == "1E");

    // The overall classification needs the worst outcome across the two
    // upstreams: NotFound only when *both* answered cleanly with no match,
    // Transient as soon as either one couldn't speak. We track Yugipedia's
    // outcome here and combine it with YGOPRODeck's below.
    bool yugipediaSawTransient = false;
    PreviewLookupError yugipediaErr{K::NotFound, "Yugipedia not consulted."};

    // Step 1: Yugipedia per-printing scan. Build a batch of plausible English
    // filenames and ask MediaWiki for them all in one call. This is the only
    // source we know of that distinguishes art between same-passcode reprints
    // (LOB Blue-Eyes vs SDK Blue-Eyes, etc.).
    //
    // No usable set code (or empty candidate list) is treated as an
    // "inapplicable" Yugipedia step rather than a failure - we don't want a
    // legitimate metadata gap to taint the final classification as transient.
    if (!setCode.empty()) {
        const auto candidates = buildCandidateFilenames(
            name, setCode, rarityCode, firstEdition);
        if (!candidates.empty()) {
            const std::string url = buildYugipediaQueryUrl(candidates);
            auto resp = http_.get(url);
            if (!resp) {
                yugipediaSawTransient = true;
                yugipediaErr = {K::Transient, resp.error()};
            } else {
                auto parsed = parseYugipediaResponse(resp.value(), candidates);
                if (parsed) return parsed;
                yugipediaErr = std::move(parsed).error();
                if (yugipediaErr.kind == K::Transient) yugipediaSawTransient = true;
            }
        }
    }

    // Step 2: YGOPRODeck standard-art fallback. Only used when Yugipedia has
    // no scan we can match (newly-added cards, OCG-only cards without an
    // English release, transient Yugipedia errors). Always unfiltered, so
    // card_images[0] is the original artwork rather than an alt-art reprint.
    const std::string fallbackUrl = buildSearchUrl(name, "");
    auto fallback = http_.get(fallbackUrl);
    if (!fallback) {
        // YGOPRODeck failed at the network layer => the overall lookup is
        // transient regardless of what Yugipedia did. Surface YGOPRODeck's
        // error string because it's the most recent failure.
        return R::err({K::Transient, fallback.error()});
    }
    auto parsed = parseFallbackImageUrl(fallback.value(), name);
    if (parsed) return parsed;

    // Both upstreams answered. If *either* one was transient, the overall
    // outcome is transient (we can't conclude the record has no image).
    PreviewLookupError fallbackErr = std::move(parsed).error();
    if (yugipediaSawTransient || fallbackErr.kind == K::Transient) {
        return R::err({K::Transient,
            yugipediaSawTransient ? yugipediaErr.message : fallbackErr.message});
    }
    // Otherwise both confirmed "no image" => safe to remember.
    return R::err({K::NotFound, fallbackErr.message});
}

Result<AutoDetectedPrint> YuGiOhCardPreviewSource::detectFirstPrint(std::string_view name,
                                                                    std::string_view setId) {
    const std::string url = buildSearchUrl(name, setId);
    auto resp = http_.get(url);
    if (resp) {
        return parseFirstPrint(resp.value(), setId);
    }
    const std::string fallbackUrl = buildSearchUrl(name, "");
    auto fallback = http_.get(fallbackUrl);
    if (!fallback) return Result<AutoDetectedPrint>::err(fallback.error());
    return parseFirstPrint(fallback.value(), setId);
}

}  // namespace ccm
