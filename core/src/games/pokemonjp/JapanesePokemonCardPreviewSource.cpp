#include "ccm/games/pokemonjp/JapanesePokemonCardPreviewSource.hpp"

#include "ccm/util/Rfc3986.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <string>
#include <unordered_set>

namespace ccm {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string asciiLower(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

std::string stripLeadingZeros(std::string_view s) {
    std::size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0') ++i;
    return std::string(s.substr(i));
}

bool localIdsMatch(std::string_view a, std::string_view b) {
    if (a == b) return true;
    return stripLeadingZeros(a) == stripLeadingZeros(b);
}

bool catalogPrintMatchesRow(const JapanesePokemonPrintEnInfo& print,
                            const JapanesePokemonCardPreviewSource::SetCardRow& row) {
    // Reject stale catalog rows whose Japanese name disagrees with TCGdex.
    // Seed data historically mapped Charmander→001 / Charizard→004; those
    // localIds are Bulbasaur / Weedle on PMCG1.
    if (print.nameJa.empty()) return true;
    return asciiLower(print.nameJa) == asciiLower(row.nameJa);
}

bool nameMatchesRow(std::string_view wantedLower,
                    const JapanesePokemonCardPreviewSource::SetCardRow& row,
                    std::string_view setId,
                    const JapanesePokemonEnCatalog& catalog) {
    if (wantedLower.empty()) return true;
    if (asciiLower(row.nameJa) == wantedLower) return true;
    if (auto print = catalog.findPrint(setId, row.localId)) {
        if (!catalogPrintMatchesRow(*print, row)) return false;
        if (asciiLower(print->nameEn) == wantedLower) return true;
        if (asciiLower(print->nameJa) == wantedLower) return true;
    }
    return false;
}

}  // namespace

JapanesePokemonCardPreviewSource::JapanesePokemonCardPreviewSource(
    IHttpClient& http, const JapanesePokemonEnCatalog& catalog)
    : http_(http), catalog_(catalog) {}

std::string JapanesePokemonCardPreviewSource::normalizeLocalId(std::string_view setNo) {
    std::string s = trim(std::string(setNo));
    const auto slash = s.find('/');
    if (slash != std::string::npos) s.erase(slash);
    return s;
}

std::string JapanesePokemonCardPreviewSource::buildSetDetailUrl(std::string_view setId) {
    return std::string("https://api.tcgdex.net/v2/ja/sets/") +
           rfc3986PercentEncode(setId);
}

std::string JapanesePokemonCardPreviewSource::buildCardUrl(std::string_view setId,
                                                           std::string_view localId) {
    std::string id = std::string(setId) + "-" + std::string(localId);
    return std::string("https://api.tcgdex.net/v2/ja/cards/") +
           rfc3986PercentEncode(id);
}

std::string JapanesePokemonCardPreviewSource::imageUrlFromBase(std::string_view imageBase) {
    if (imageBase.empty()) return {};
    std::string url(imageBase);
    while (!url.empty() && (url.back() == '/' || url.back() == ' ')) url.pop_back();
    return url + "/high.png";
}

Result<std::vector<JapanesePokemonCardPreviewSource::SetCardRow>, PreviewLookupError>
JapanesePokemonCardPreviewSource::parseSetCards(const std::string& body) {
    using R = Result<std::vector<SetCardRow>, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_object() || !j.contains("cards") || !j.at("cards").is_array()) {
            return R::err({K::Transient,
                           "TCGdex JA set detail missing 'cards' array."});
        }
        std::vector<SetCardRow> out;
        out.reserve(j.at("cards").size());
        for (const auto& card : j.at("cards")) {
            SetCardRow row;
            row.localId = card.value("localId", "");
            if (row.localId.empty() && card.contains("id") && card.at("id").is_string()) {
                // Fallback: take suffix after last '-' from card id.
                const std::string id = card.at("id").get<std::string>();
                const auto dash = id.rfind('-');
                if (dash != std::string::npos) row.localId = id.substr(dash + 1);
            }
            row.nameJa = card.value("name", "");
            row.rarity = card.value("rarity", "");
            if (card.contains("image") && card.at("image").is_string()) {
                row.imageBase = card.at("image").get<std::string>();
            }
            if (row.localId.empty()) continue;
            out.push_back(std::move(row));
        }
        return R::ok(std::move(out));
    } catch (const std::exception& e) {
        return R::err({K::Transient,
                       std::string("TCGdex JA set detail JSON parse error: ") + e.what()});
    }
}

Result<std::string, PreviewLookupError>
JapanesePokemonCardPreviewSource::parseCardImageUrl(const std::string& body) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_object()) {
            return R::err({K::Transient, "TCGdex JA card response is not a JSON object."});
        }
        if (!j.contains("image") || j.at("image").is_null()) {
            return R::err({K::NotFound, "TCGdex JA card has no image."});
        }
        if (!j.at("image").is_string()) {
            return R::err({K::Transient, "TCGdex JA card image field is not a string."});
        }
        const std::string base = j.at("image").get<std::string>();
        if (base.empty()) {
            return R::err({K::NotFound, "TCGdex JA card has no image."});
        }
        return R::ok(imageUrlFromBase(base));
    } catch (const std::exception& e) {
        return R::err({K::Transient,
                       std::string("TCGdex JA card JSON parse error: ") + e.what()});
    }
}

Result<std::vector<AutoDetectedPrint>>
JapanesePokemonCardPreviewSource::parsePrintVariants(
    const std::string& body,
    std::string_view setId,
    std::string_view wantedCardName,
    const JapanesePokemonEnCatalog& catalog) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    auto rows = parseSetCards(body);
    if (!rows) {
        return R::err(rows.error().message);
    }

    const std::string wantedLower = asciiLower(trim(std::string(wantedCardName)));
    std::vector<AutoDetectedPrint> out;
    std::unordered_set<std::string> seen;
    std::unordered_set<std::string> seenCatalogUrls;

    // Prefer catalog EN matches first so typed English names resolve — but
    // only when the catalog localId exists in the set and name_ja agrees
    // with TCGdex (guards against stale seed mappings).
    std::vector<AutoDetectedPrint> withPreview;
    std::vector<AutoDetectedPrint> withoutPreview;
    if (!wantedLower.empty()) {
        for (const auto& p : catalog.findPrintsByName(setId, wantedCardName)) {
            const SetCardRow* row = nullptr;
            for (const auto& r : rows.value()) {
                if (localIdsMatch(r.localId, p.localId)) {
                    row = &r;
                    break;
                }
            }
            const std::string previewUrl =
                JapanesePokemonEnCatalog::previewImageUrlFromPrint(p);
            if (row == nullptr) {
                // Set detail sometimes omits cards[]; keep catalog-only hits
                // (UnnumberedPromo). Dedupe only among non-empty preview URLs
                // so empty-image prints still appear in the Next ring.
                if (!rows.value().empty()) continue;
                if (!previewUrl.empty() &&
                    !seenCatalogUrls.insert(previewUrl).second) {
                    continue;
                }
            } else if (!catalogPrintMatchesRow(p, *row)) {
                continue;
            }
            if (!seen.insert(p.localId).second) continue;
            AutoDetectedPrint print;
            print.setNo = p.localId;
            if (row != nullptr) print.rarity = row->rarity;
            if (previewUrl.empty()) {
                withoutPreview.push_back(std::move(print));
            } else {
                withPreview.push_back(std::move(print));
            }
        }
    }

    for (auto& print : withPreview) out.push_back(std::move(print));
    for (auto& print : withoutPreview) out.push_back(std::move(print));

    for (const auto& row : rows.value()) {
        if (!nameMatchesRow(wantedLower, row, setId, catalog)) continue;
        if (!seen.insert(row.localId).second) continue;
        AutoDetectedPrint print;
        print.setNo = row.localId;
        print.rarity = row.rarity;
        out.push_back(std::move(print));
    }

    if (out.empty() && !wantedLower.empty()) {
        return R::err("No matching Japanese Pokemon prints for that name in the set.");
    }
    return R::ok(std::move(out));
}

Result<std::vector<AutoDetectedPrint>>
JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
    std::string_view setId,
    std::string_view wantedCardName,
    const JapanesePokemonEnCatalog& catalog) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    if (!catalog.hasPrintsForSet(setId)) {
        return R::err("No matching Japanese Pokemon prints for that name in the set.");
    }
    const std::string wantedLower = asciiLower(trim(std::string(wantedCardName)));
    std::vector<AutoDetectedPrint> out;
    std::unordered_set<std::string> seen;
    std::unordered_set<std::string> seenUrls;
    if (wantedLower.empty()) {
        return R::ok(std::move(out));
    }
    std::vector<AutoDetectedPrint> withPreview;
    std::vector<AutoDetectedPrint> withoutPreview;
    for (const auto& p : catalog.findPrintsByName(setId, wantedCardName)) {
        if (!seen.insert(p.localId).second) continue;
        // Dedupe only among non-empty preview URLs so identical art is not
        // cycled; empty-image prints still join the Next ring (card-back).
        // Emit imaged prints first so Auto-detect lands on real art.
        const std::string previewUrl =
            JapanesePokemonEnCatalog::previewImageUrlFromPrint(p);
        if (!previewUrl.empty() && !seenUrls.insert(previewUrl).second) {
            continue;
        }
        AutoDetectedPrint print;
        print.setNo = p.localId;
        if (previewUrl.empty()) {
            withoutPreview.push_back(std::move(print));
        } else {
            withPreview.push_back(std::move(print));
        }
    }
    for (auto& print : withPreview) out.push_back(std::move(print));
    for (auto& print : withoutPreview) out.push_back(std::move(print));
    if (out.empty()) {
        return R::err("No matching Japanese Pokemon prints for that name in the set.");
    }
    return R::ok(std::move(out));
}

Result<std::string, PreviewLookupError>
JapanesePokemonCardPreviewSource::fetchImageUrl(std::string_view name,
                                                std::string_view setId,
                                                std::string_view setNo) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;

    const std::string localId = normalizeLocalId(setNo);
    if (setId.empty()) {
        return R::err({K::NotFound, "Japanese Pokemon preview requires a set id."});
    }

    auto catalogPreviewFor = [&](std::string_view lid) -> Result<std::string, PreviewLookupError> {
        if (lid.empty()) return R::err({K::NotFound, "No catalog preview for print."});
        if (auto print = catalog_.findPrint(setId, lid)) {
            const std::string catalogUrl =
                JapanesePokemonEnCatalog::previewImageUrlFromPrint(*print);
            if (!catalogUrl.empty()) return R::ok(catalogUrl);
        }
        return R::err({K::NotFound, "TCGdex JA card has no image."});
    };

    // Prefer direct card fetch when we have a localId.
    if (!localId.empty()) {
        auto cardResp = http_.get(buildCardUrl(setId, localId));
        if (cardResp) {
            auto img = parseCardImageUrl(cardResp.value());
            if (img) return img;
            // NotFound from card object: fall through to set list / catalog.
            if (img.error().kind == K::Transient) return img;
        } else {
            // Synthetic / classic products: try catalog gap-fill before set detail.
            auto catalogImg = catalogPreviewFor(localId);
            if (catalogImg) return catalogImg;
            // Known catalog print with no preview URL: honest miss (do not
            // borrow a sibling print's art via name match).
            if (catalog_.findPrint(setId, localId)) {
                return R::err({K::NotFound, "TCGdex JA card has no image."});
            }
        }
    }

    auto setResp = http_.get(buildSetDetailUrl(setId));
    if (!setResp) {
        // Catalog-only products (City Gym theme decks, etc.) are not on TCGdex.
        if (!localId.empty()) {
            auto catalogImg = catalogPreviewFor(localId);
            if (catalogImg) return catalogImg;
            if (catalog_.findPrint(setId, localId)) {
                return R::err({K::NotFound, "TCGdex JA card has no image."});
            }
            // Specific setNo was requested but is unknown in catalog: do not
            // steal another printing's image by English name.
            return R::err({K::NotFound, "No matching Japanese Pokemon card for preview."});
        }
        if (catalog_.hasPrintsForSet(setId)) {
            const std::string wantedLower = asciiLower(trim(std::string(name)));
            if (!wantedLower.empty()) {
                for (const auto& p : catalog_.findPrintsByName(setId, name)) {
                    auto catalogImg = catalogPreviewFor(p.localId);
                    if (catalogImg) return catalogImg;
                }
            }
            return R::err({K::NotFound, "No matching Japanese Pokemon card for preview."});
        }
        return R::err({K::Transient, setResp.error()});
    }
    auto rows = parseSetCards(setResp.value());
    if (!rows) return R::err(rows.error());

    const std::string wantedLower = asciiLower(trim(std::string(name)));
    const SetCardRow* chosen = nullptr;
    for (const auto& row : rows.value()) {
        if (!localId.empty() && localIdsMatch(row.localId, localId)) {
            chosen = &row;
            break;
        }
    }
    // Name match only when setNo was not provided — never borrow a sibling
    // print's art for a concrete localId.
    if (chosen == nullptr && localId.empty() && !wantedLower.empty()) {
        for (const auto& row : rows.value()) {
            if (nameMatchesRow(wantedLower, row, setId, catalog_)) {
                chosen = &row;
                break;
            }
        }
    }
    if (chosen == nullptr) {
        // Empty cards[] with catalog prints: resolve from catalog.
        if (rows.value().empty() && catalog_.hasPrintsForSet(setId)) {
            if (!localId.empty()) {
                auto catalogImg = catalogPreviewFor(localId);
                if (catalogImg) return catalogImg;
                if (catalog_.findPrint(setId, localId)) {
                    return R::err({K::NotFound, "TCGdex JA card has no image."});
                }
                return R::err({K::NotFound, "No matching Japanese Pokemon card for preview."});
            }
            if (!wantedLower.empty()) {
                for (const auto& p : catalog_.findPrintsByName(setId, name)) {
                    auto catalogImg = catalogPreviewFor(p.localId);
                    if (catalogImg) return catalogImg;
                }
            }
        }
        return R::err({K::NotFound, "No matching Japanese Pokemon card for preview."});
    }
    if (!chosen->imageBase.empty()) {
        return R::ok(imageUrlFromBase(chosen->imageBase));
    }

    // Try full card object — set résumé sometimes omits image.
    auto cardResp = http_.get(buildCardUrl(setId, chosen->localId));
    if (cardResp) {
        auto img = parseCardImageUrl(cardResp.value());
        if (img) return img;
        if (img.error().kind == K::Transient) return img;
    } else {
        // Odd localId padding can 404; still try catalog gap-fill below.
    }

    // Classic JA sets often have image:null on TCGdex. Prefer a catalog
    // printing-accurate TCGPlayer product image for this exact setId+localId
    // (never search other printings by Pokémon name).
    return catalogPreviewFor(chosen->localId);
}

Result<AutoDetectedPrint>
JapanesePokemonCardPreviewSource::detectFirstPrint(std::string_view name,
                                                   std::string_view setId) {
    auto variants = detectPrintVariants(name, setId);
    if (!variants) return Result<AutoDetectedPrint>::err(variants.error());
    if (variants.value().empty()) {
        return Result<AutoDetectedPrint>::err(
            "No matching Japanese Pokemon prints for that name in the set.");
    }
    return Result<AutoDetectedPrint>::ok(variants.value().front());
}

Result<std::vector<AutoDetectedPrint>>
JapanesePokemonCardPreviewSource::detectPrintVariants(std::string_view name,
                                                      std::string_view setId) {
    if (setId.empty()) {
        return Result<std::vector<AutoDetectedPrint>>::err(
            "Select a set before auto-detecting Japanese Pokemon prints.");
    }
    auto setResp = http_.get(buildSetDetailUrl(setId));
    if (!setResp) {
        if (catalog_.hasPrintsForSet(setId)) {
            return detectPrintVariantsFromCatalog(setId, name, catalog_);
        }
        return Result<std::vector<AutoDetectedPrint>>::err(setResp.error());
    }
    auto parsed = parsePrintVariants(setResp.value(), setId, name, catalog_);
    if (parsed) return parsed;
    // Empty/unusable TCGdex detail: fall back to catalog prints when present.
    if (catalog_.hasPrintsForSet(setId)) {
        return detectPrintVariantsFromCatalog(setId, name, catalog_);
    }
    return parsed;
}

}  // namespace ccm
