#include "ccm/games/yugiohbandai/YuGiOhBandaiCardPreviewSource.hpp"

#include "ccm/games/yugiohbandai/YuGiOhBandaiSetSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ccm {

namespace {

using K = PreviewLookupError::Kind;

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

std::string wikiTitleEncode(std::string_view title) {
    // MediaWiki titles use underscores for spaces in the titles= parameter.
    std::string s;
    s.reserve(title.size());
    for (char c : title) {
        s.push_back(c == ' ' ? '_' : c);
    }
    return urlEncode(s);
}

bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int askMatchRank(std::string_view pageTitle, std::string_view preferredSetId) {
    // Lower is better.
    if (preferredSetId == "bansealdass") {
        if (endsWith(pageTitle, " (Bandai Sealdass)")) return 0;
        if (endsWith(pageTitle, " (Bandai)")) return 1;
        return 5;
    }
    if (preferredSetId == "ban3") {
        if (endsWith(pageTitle, " (Bandai)")) return 0;
        if (endsWith(pageTitle, " (English Bandai)")) return 1;
        if (endsWith(pageTitle, " (Bandai Sealdass)")) return 4;
        return 5;
    }
    if (endsWith(pageTitle, " (Bandai)")) return 0;
    if (endsWith(pageTitle, " (English Bandai)")) return 1;
    if (endsWith(pageTitle, " (Bandai Sealdass)")) return 3;
    return 5;
}

}  // namespace

YuGiOhBandaiCardPreviewSource::YuGiOhBandaiCardPreviewSource(IHttpClient& http)
    : http_(http) {}

std::string YuGiOhBandaiCardPreviewSource::preferredPageTitle(
    std::string_view name,
    std::string_view setId,
    std::string_view setNo) {
    const std::string n = trimCopy(name);
    if (n.empty()) return {};

    const std::string num = YuGiOhBandaiSetSource::normalizeCardNumber(setNo);
    if (setId == "bansealdass") {
        return n + " (Bandai Sealdass)";
    }
    // Promo pages on Yugipedia often omit the "(Bandai)" disambiguator
    // (e.g. Blue-Eyes White Dragon's 3-Body Connection for TA2).
    if (setId == "banpromo-j" || setId == "banpromo-ta" ||
        isAlphanumericPromoNumber(num)) {
        return n;
    }
    if (num == "118" || setId == "ban3") {
        // Prefer JP Bandai page for most ban3 cards; English #118 uses the
        // English Bandai title when setNo is 118.
        if (num == "118") return n + " (English Bandai)";
    }
    return n + " (Bandai)";
}

std::string YuGiOhBandaiCardPreviewSource::buildPageImagesUrl(
    std::string_view pageTitle) {
    return std::string(
               "https://yugipedia.com/api.php?action=query&format=json"
               "&prop=pageimages&piprop=original&titles=") +
           wikiTitleEncode(pageTitle);
}

std::string YuGiOhBandaiCardPreviewSource::buildAskByNameUrl(
    std::string_view englishName) {
    // [[Category:Bandai cards]][[English name::<name>]]|?English name|?Bandai number|?Rarity|limit=20
    std::ostringstream q;
    q << "[[Category:Bandai cards]][[English name::" << englishName
      << "]]|?English name|?Bandai number|?Rarity|limit=20";
    return std::string("https://yugipedia.com/api.php?action=ask&format=json&query=") +
           urlEncode(q.str());
}

std::string YuGiOhBandaiCardPreviewSource::buildAskByNumberUrl(
    std::string_view setNo) {
    const std::string n = YuGiOhBandaiSetSource::normalizeCardNumber(setNo);
    std::ostringstream q;
    q << "[[Category:Bandai cards]][[Bandai number::" << n
      << "]]|?English name|?Bandai number|?Rarity|limit=20";
    return std::string("https://yugipedia.com/api.php?action=ask&format=json&query=") +
           urlEncode(q.str());
}

bool YuGiOhBandaiCardPreviewSource::isAlphanumericPromoNumber(
    std::string_view setNo) {
    const std::string n = YuGiOhBandaiSetSource::normalizeCardNumber(setNo);
    for (unsigned char c : n) {
        if (std::isalpha(c)) return true;
    }
    return false;
}

Result<std::vector<AutoDetectedPrint>>
YuGiOhBandaiCardPreviewSource::parsePromoGalleryResponse(
    const std::string& body,
    std::string_view wantedSetNo) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    const std::string want = YuGiOhBandaiSetSource::normalizeCardNumber(wantedSetNo);
    if (want.empty()) return R::err("Card number is empty.");

    std::string wikitext;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("parse") || !j.at("parse").contains("wikitext")) {
            return R::err("Yugipedia promo gallery response missing parse.wikitext");
        }
        wikitext = j.at("parse").at("wikitext").get<std::string>();
    } catch (const std::exception& e) {
        return R::err(std::string("Yugipedia promo gallery JSON parse error: ") +
                      e.what());
    }

    auto cards = YuGiOhBandaiSetSource::parseGalleryWikitext(wikitext);
    if (!cards) return R::err(cards.error());

    std::vector<AutoDetectedPrint> out;
    for (const auto& card : cards.value()) {
        if (YuGiOhBandaiSetSource::normalizeCardNumber(card.setNo) != want) continue;
        AutoDetectedPrint print;
        print.name = card.name;
        print.setNo = card.setNo;
        print.rarity = card.rarity;
        print.setId = YuGiOhBandaiSetSource::setIdForNumber(card.setNo);
        print.setName = YuGiOhBandaiSetSource::setNameForId(print.setId);
        print.language = "Japanese";
        out.push_back(std::move(print));
    }
    return R::ok(std::move(out));
}

AutoDetectedPrint YuGiOhBandaiCardPreviewSource::enrichPrint(
    AutoDetectedPrint print,
    std::string_view pageTitle) {
    print.name = YuGiOhBandaiSetSource::englishNameFromGalleryTitle(pageTitle);

    if (endsWith(pageTitle, " (Bandai Sealdass)")) {
        print.setId = "bansealdass";
        print.language = "Japanese";
    } else if (endsWith(pageTitle, " (English Bandai)")) {
        print.setId = "ban3";
        print.language = "English";
    } else {
        if (print.setId.empty() && !print.setNo.empty()) {
            print.setId = YuGiOhBandaiSetSource::setIdForNumber(print.setNo);
        }
        print.language = "Japanese";
    }
    if (!print.setId.empty()) {
        print.setName = YuGiOhBandaiSetSource::setNameForId(print.setId);
    }
    return print;
}

Result<std::string, PreviewLookupError>
YuGiOhBandaiCardPreviewSource::parsePageImagesResponse(const std::string& body) {
    using R = Result<std::string, PreviewLookupError>;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("query") || !j.at("query").contains("pages")) {
            return R::err({K::Transient, "Yugipedia pageimages: missing query.pages"});
        }
        const auto& pages = j.at("query").at("pages");
        for (auto it = pages.begin(); it != pages.end(); ++it) {
            const auto& page = it.value();
            if (page.contains("missing") || page.contains("invalid")) continue;
            if (page.contains("original") && page.at("original").contains("source")) {
                const auto url = page.at("original").at("source").get<std::string>();
                if (!url.empty()) return R::ok(url);
            }
            if (page.contains("thumbnail") && page.at("thumbnail").contains("original")) {
                const auto url = page.at("thumbnail").at("original").get<std::string>();
                if (!url.empty()) return R::ok(url);
            }
        }
        return R::err({K::NotFound, "Yugipedia pageimages: no image for page"});
    } catch (const std::exception& e) {
        return R::err({K::Transient,
                       std::string("Yugipedia pageimages JSON parse error: ") + e.what()});
    }
}

Result<std::vector<AutoDetectedPrint>>
YuGiOhBandaiCardPreviewSource::parseAskResponse(const std::string& body,
                                                std::string_view preferredSetId) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("query") || !j.at("query").contains("results")) {
            return R::err("Yugipedia ask: missing query.results");
        }
        const auto& results = j.at("query").at("results");
        if (!results.is_object() || results.empty()) {
            return R::ok({});
        }

        std::vector<std::pair<int, AutoDetectedPrint>> ranked;
        for (auto it = results.begin(); it != results.end(); ++it) {
            const std::string pageTitle = it.key();
            const auto& printouts = it.value().value("printouts", nlohmann::json::object());

            AutoDetectedPrint print;
            if (printouts.contains("Bandai number") &&
                printouts.at("Bandai number").is_array() &&
                !printouts.at("Bandai number").empty()) {
                const auto& num = printouts.at("Bandai number").at(0);
                if (num.is_number_integer()) {
                    print.setNo = YuGiOhBandaiSetSource::normalizeCardNumber(
                        std::to_string(num.get<int>()));
                } else if (num.is_string()) {
                    print.setNo =
                        YuGiOhBandaiSetSource::normalizeCardNumber(num.get<std::string>());
                }
            }
            if (printouts.contains("Rarity") && printouts.at("Rarity").is_array() &&
                !printouts.at("Rarity").empty()) {
                const auto& rar = printouts.at("Rarity").at(0);
                if (rar.is_object() && rar.contains("fulltext")) {
                    print.rarity = rar.at("fulltext").get<std::string>();
                } else if (rar.is_string()) {
                    print.rarity = rar.get<std::string>();
                }
            }
            if (printouts.contains("English name") &&
                printouts.at("English name").is_array() &&
                !printouts.at("English name").empty()) {
                print.name = printouts.at("English name").at(0).get<std::string>();
            }

            print = enrichPrint(std::move(print), pageTitle);
            if (print.name.empty()) continue;
            ranked.emplace_back(askMatchRank(pageTitle, preferredSetId), std::move(print));
        }

        std::sort(ranked.begin(), ranked.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        std::vector<AutoDetectedPrint> out;
        out.reserve(ranked.size());
        for (auto& [rank, print] : ranked) {
            (void)rank;
            out.push_back(std::move(print));
        }
        return R::ok(std::move(out));
    } catch (const std::exception& e) {
        return R::err(std::string("Yugipedia ask JSON parse error: ") + e.what());
    }
}

Result<std::string, PreviewLookupError>
YuGiOhBandaiCardPreviewSource::fetchPageImage(std::string_view pageTitle) {
    using R = Result<std::string, PreviewLookupError>;
    if (pageTitle.empty()) {
        return R::err({K::NotFound, "Empty Bandai page title"});
    }
    const std::string url = buildPageImagesUrl(pageTitle);
    auto resp = http_.get(url);
    if (!resp) return R::err({K::Transient, resp.error()});
    return parsePageImagesResponse(resp.value());
}

Result<std::vector<AutoDetectedPrint>> YuGiOhBandaiCardPreviewSource::askByName(
    std::string_view name,
    std::string_view setId) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    const std::string n = trimCopy(name);
    if (n.empty()) return R::err("Card name is empty.");
    const std::string url = buildAskByNameUrl(n);
    auto resp = http_.get(url);
    if (!resp) return R::err(resp.error());
    return parseAskResponse(resp.value(), setId);
}

Result<std::vector<AutoDetectedPrint>> YuGiOhBandaiCardPreviewSource::askByNumber(
    std::string_view setNo) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    const std::string n = YuGiOhBandaiSetSource::normalizeCardNumber(setNo);
    if (n.empty()) return R::err("Card number is empty.");

    // Promo codes (J1, TA2, …) are not valid values for SMW's numeric
    // `Bandai number` property — ask returns a type error. Resolve them from
    // the promotional set gallery instead.
    if (isAlphanumericPromoNumber(n)) {
        static constexpr const char* kPromoGallery =
            "Set Card Galleries:Promotional Cards (Bandai)";
        const std::string url = YuGiOhBandaiSetSource::buildGalleryParseUrl(kPromoGallery);
        auto resp = http_.get(url);
        if (!resp) return R::err(resp.error());
        return parsePromoGalleryResponse(resp.value(), n);
    }

    const std::string url = buildAskByNumberUrl(n);
    auto resp = http_.get(url);
    if (!resp) return R::err(resp.error());
    return parseAskResponse(resp.value(), {});
}

Result<std::string, PreviewLookupError>
YuGiOhBandaiCardPreviewSource::fetchImageUrl(std::string_view name,
                                             std::string_view setId,
                                             std::string_view setNo) {
    using R = Result<std::string, PreviewLookupError>;

    const std::string title = preferredPageTitle(name, setId, setNo);
    auto direct = fetchPageImage(title);
    if (direct) return direct;
    // Try English Bandai if JP page missed for #118.
    if (YuGiOhBandaiSetSource::normalizeCardNumber(setNo) == "118") {
        auto en = fetchPageImage(trimCopy(name) + " (English Bandai)");
        if (en) return en;
    }

    // Fall back to SMW ask by name, then pageimages on the best hit.
    auto variants = askByName(name, setId);
    if (!variants) {
        // Prefer the original NotFound if ask also failed transiently only
        // after a clean miss; otherwise surface ask error as Transient.
        if (direct.error().kind == K::NotFound) {
            return R::err({K::Transient, variants.error()});
        }
        return direct;
    }
    if (variants.value().empty()) {
        return R::err({K::NotFound, "No Bandai card matched the name"});
    }

    const auto& best = variants.value().front();
    std::string askTitle = preferredPageTitle(best.name, best.setId, best.setNo);
    if (best.language == "English") {
        askTitle = best.name + " (English Bandai)";
    } else if (best.setId == "bansealdass") {
        askTitle = best.name + " (Bandai Sealdass)";
    }
    return fetchPageImage(askTitle);
}

Result<AutoDetectedPrint> YuGiOhBandaiCardPreviewSource::detectFirstPrint(
    std::string_view name,
    std::string_view setId) {
    auto list = detectPrintVariants(name, setId);
    if (!list) return Result<AutoDetectedPrint>::err(list.error());
    if (list.value().empty()) {
        return Result<AutoDetectedPrint>::err("Could not auto-detect Bandai print metadata.");
    }
    return Result<AutoDetectedPrint>::ok(list.value().front());
}

Result<std::vector<AutoDetectedPrint>>
YuGiOhBandaiCardPreviewSource::detectPrintVariants(std::string_view name,
                                                    std::string_view setId) {
    return askByName(name, setId);
}

Result<AutoDetectedPrint> YuGiOhBandaiCardPreviewSource::detectBySetNo(
    std::string_view setNo) {
    auto list = detectVariantsBySetNo(setNo);
    if (!list) return Result<AutoDetectedPrint>::err(list.error());
    if (list.value().empty()) {
        return Result<AutoDetectedPrint>::err(
            "Could not auto-detect Bandai card from number.");
    }
    return Result<AutoDetectedPrint>::ok(list.value().front());
}

Result<std::vector<AutoDetectedPrint>>
YuGiOhBandaiCardPreviewSource::detectVariantsBySetNo(std::string_view setNo) {
    return askByNumber(setNo);
}

}  // namespace ccm
