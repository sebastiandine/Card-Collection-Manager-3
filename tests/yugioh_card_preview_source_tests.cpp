#include <doctest/doctest.h>

#include "ccm/games/yugioh/YuGiOhCardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"
#include "ccm/util/YuGiOhPrintingSlot.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace ccm;

namespace {

// Single-shot HTTP fake matching the other game tests. Captures the last URL
// requested and returns a configurable body / failure flag.
class FixedHttpClient final : public IHttpClient {
public:
    std::string lastUrl;
    std::string body;
    bool ok = true;
    Result<std::string> get(std::string_view url) override {
        lastUrl = std::string(url);
        return ok ? Result<std::string>::ok(body)
                  : Result<std::string>::err("offline");
    }
};

// Routing fake used to simulate the two-step Yugipedia -> YGOPRODeck flow.
// Both bodies are queued by URL substring so order-of-call assertions stay
// readable in the tests. Unmatched URLs return a 404-equivalent error so
// regressions are obvious instead of silently passing.
class RoutingHttpClient final : public IHttpClient {
public:
    std::string yugipediaBody;
    std::string ygoprodeckBody;
    bool yugipediaOk = true;
    bool ygoprodeckOk = true;
    std::vector<std::string> calls;
    Result<std::string> get(std::string_view url) override {
        std::string s(url);
        calls.push_back(s);
        if (s.find("yugipedia.com") != std::string::npos) {
            return yugipediaOk ? Result<std::string>::ok(yugipediaBody)
                               : Result<std::string>::err("yugipedia offline");
        }
        if (s.find("ygoprodeck.com") != std::string::npos) {
            return ygoprodeckOk ? Result<std::string>::ok(ygoprodeckBody)
                                : Result<std::string>::err("ygoprodeck offline");
        }
        return Result<std::string>::err("RoutingHttpClient: unrouted URL");
    }
};

// First GET (filtered `cardset=` URL) fails; second GET (unfiltered) succeeds.
// Exercises `YuGiOhCardPreviewSource::detectPrintVariants` narrow-query fallback.
class FailFilteredThenOkHttpClient final : public IHttpClient {
public:
    std::string unfilteredBody;
    int calls{0};

    Result<std::string> get(std::string_view url) override {
        ++calls;
        std::string u(url);
        if (u.find("cardset=") != std::string::npos) {
            return Result<std::string>::err("filtered endpoint unavailable");
        }
        return Result<std::string>::ok(unfilteredBody);
    }
};

}  // namespace

TEST_SUITE("ygoPrintingSlotsMatch") {
    TEST_CASE("matches CCM composed setNo to YGOPRODeck set_code with region infix") {
        CHECK(ygoPrintingSlotsMatch("SOD-015", "SOD-EN015"));
        CHECK(ygoPrintingSlotsMatch("sod-015", "SOD-EN015"));
        CHECK_FALSE(ygoPrintingSlotsMatch("SOD-015", "SOD-EN016"));
        CHECK_FALSE(ygoPrintingSlotsMatch("SOD-015", "IOC-EN015"));
    }

    TEST_CASE("matches German-style and digits-only suffix variants") {
        CHECK(ygoPrintingSlotsMatch("LOB-005", "LOB-DE005"));
        CHECK(ygoPrintingSlotsMatch("RA04-001", "RA04-EN001"));
    }

    TEST_CASE("detects European alternate numbering suffix E+digit vs EN/DE") {
        CHECK(ygoLikelyEuropeanRegionalSetCode("LOB-E003"));
        CHECK_FALSE(ygoLikelyEuropeanRegionalSetCode("LOB-EN005"));
        CHECK_FALSE(ygoLikelyEuropeanRegionalSetCode("LOB-005"));
        CHECK_FALSE(ygoLikelyEuropeanRegionalSetCode("LOB-DE005"));
        CHECK_FALSE(ygoLikelyEuropeanRegionalSetCode("SOD-EN015"));
    }
}

TEST_SUITE("ygoRarityShortCode") {
    TEST_CASE("maps supported Yu-Gi-Oh rarity names to short form") {
        CHECK(ygoRarityShortCode("Common") == "C");
        CHECK(ygoRarityShortCode("Rare") == "R");
        CHECK(ygoRarityShortCode("Super Rare") == "SR");
        CHECK(ygoRarityShortCode("Ultra Rare") == "UR");
        CHECK(ygoRarityShortCode("Secret Rare") == "ScR");
        CHECK(ygoRarityShortCode("Quarter Century Secret Rare") == "QCScR");
        CHECK(ygoRarityShortCode("Starlight Rare") == "StR");
        CHECK(ygoRarityShortCode("Collector's Rare") == "CR");
        CHECK(ygoRarityShortCode("Ghost Rare") == "GR");
        CHECK(ygoRarityShortCode("Ultimate Rare") == "UtR");
        CHECK(ygoRarityShortCode("Platinum Secret Rare") == "PlScR");
        CHECK(ygoRarityShortCode("Prismatic Secret Rare") == "PScR");
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::normalizeName") {
    TEST_CASE("strips whitespace and policy-banned punctuation") {
        // Yugipedia's image policy: whitespace and a fixed punctuation set
        // (#,.:'"?!&@%=[]<>/\- and friends) get dropped from the slug.
        CHECK(YuGiOhCardPreviewSource::normalizeName("Blue-Eyes White Dragon")
              == "BlueEyesWhiteDragon");
        CHECK(YuGiOhCardPreviewSource::normalizeName("Dark Magician") == "DarkMagician");
        CHECK(YuGiOhCardPreviewSource::normalizeName("Sasuke Samurai #4")
              == "SasukeSamurai4");
        CHECK(YuGiOhCardPreviewSource::normalizeName("Don't Talk to Me!")
              == "DontTalktoMe");
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::rarityCodeFor") {
    TEST_CASE("maps the dialog's rarity options to Yugipedia codes") {
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Common") == "C");
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Rare") == "R");
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Super Rare") == "SR");
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Ultra Rare") == "UR");
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Secret Rare") == "ScR");
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Quarter Century Secret Rare")
              == "QCScR");
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Collector's Rare") == "CR");
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Platinum Secret Rare") == "PlScR");
    }
    TEST_CASE("returns empty string for unknown rarity names") {
        // Unknown rarity should fall through to the rarity-less filename
        // pattern, not throw and not return a misleading code.
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("").empty());
        CHECK(YuGiOhCardPreviewSource::rarityCodeFor("Mythic Cosmic Rare").empty());
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::extractSetCode") {
    TEST_CASE("returns everything before the first dash") {
        CHECK(YuGiOhCardPreviewSource::extractSetCode("LOB-005") == "LOB");
        CHECK(YuGiOhCardPreviewSource::extractSetCode("LOB-DE005") == "LOB");
        CHECK(YuGiOhCardPreviewSource::extractSetCode("RA04-EN001") == "RA04");
    }
    TEST_CASE("returns the input unchanged when no dash is present") {
        CHECK(YuGiOhCardPreviewSource::extractSetCode("LOB") == "LOB");
        CHECK(YuGiOhCardPreviewSource::extractSetCode("").empty());
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::buildCandidateFilenames") {
    TEST_CASE("primary candidate uses the printed edition + EN region + .png") {
        const auto names = YuGiOhCardPreviewSource::buildCandidateFilenames(
            "Blue-Eyes White Dragon", "LOB", "UR", /*firstEdition=*/false);
        REQUIRE_FALSE(names.empty());
        // Highest-priority filename: Yugipedia's modern English/UE/.png
        // shape, which is what most LOB-era reprints actually use.
        CHECK(names.front() == "BlueEyesWhiteDragon-LOB-EN-UR-UE.png");
    }
    TEST_CASE("includes 1E when the user marked the card as first edition") {
        const auto names = YuGiOhCardPreviewSource::buildCandidateFilenames(
            "Dark Magician", "SDY", "UR", /*firstEdition=*/true);
        REQUIRE_FALSE(names.empty());
        CHECK(names.front() == "DarkMagician-SDY-EN-UR-1E.png");
    }
    TEST_CASE("covers EN/NA/EU/AU regions and both png/jpg extensions") {
        const auto names = YuGiOhCardPreviewSource::buildCandidateFilenames(
            "Blue-Eyes White Dragon", "SDK", "UR", /*firstEdition=*/false);
        // Sanity: the standard SDK Blue-Eyes scan is hosted at NA-UR-UE.png,
        // so the candidate list must include that exact filename.
        bool foundNaUe = false;
        for (const auto& n : names) {
            if (n == "BlueEyesWhiteDragon-SDK-NA-UR-UE.png") foundNaUe = true;
        }
        CHECK(foundNaUe);
    }
    TEST_CASE("falls back to a rarity-less pattern for unknown rarities") {
        const auto names = YuGiOhCardPreviewSource::buildCandidateFilenames(
            "Token Card", "TKN", /*rarityCode=*/"", /*firstEdition=*/false);
        REQUIRE_FALSE(names.empty());
        // Rarity slot omitted -> filename has only one dash before edition.
        CHECK(names.front() == "TokenCard-TKN-EN-UE.png");
    }
    TEST_CASE("returns empty list when the slug or set code is empty") {
        // Don't waste an HTTP call on cards that haven't been filled in yet.
        CHECK(YuGiOhCardPreviewSource::buildCandidateFilenames("", "LOB", "UR", false).empty());
        CHECK(YuGiOhCardPreviewSource::buildCandidateFilenames("Dark Magician", "", "UR", false).empty());
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::buildYugipediaQueryUrl") {
    TEST_CASE("encodes filenames into a single MediaWiki batch query") {
        const std::vector<std::string> names = {
            "BlueEyesWhiteDragon-LOB-EN-UR-UE.png",
            "BlueEyesWhiteDragon-LOB-NA-UR-UE.png",
        };
        const std::string url = YuGiOhCardPreviewSource::buildYugipediaQueryUrl(names);
        CHECK(url.find("https://yugipedia.com/api.php") == 0);
        CHECK(url.find("action=query") != std::string::npos);
        CHECK(url.find("prop=imageinfo") != std::string::npos);
        CHECK(url.find("iiprop=url") != std::string::npos);
        // The two filenames are joined with `|` (URL-encoded as %7C) and
        // each one is namespaced with `File:` (encoded `File%3A`).
        CHECK(url.find("File%3ABlueEyesWhiteDragon-LOB-EN-UR-UE.png") != std::string::npos);
        CHECK(url.find("%7CFile%3ABlueEyesWhiteDragon-LOB-NA-UR-UE.png") != std::string::npos);
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::parseYugipediaResponse") {
    TEST_CASE("returns the URL of the highest-priority filename that exists") {
        // MediaWiki returns one entry per requested title. Missing files come
        // back with `"missing": ""` and no imageinfo; existing files carry an
        // imageinfo[0].url. Order matters: we must walk filenameOrder and
        // pick the first entry that resolved, not whichever MediaWiki listed
        // first in its hash-keyed `pages` object.
        const std::string body = R"({
          "query":{"pages":{
            "-1":{"title":"File:DarkMagician-SDY-EN-UR-1E.png","missing":""},
            "96018":{"title":"File:DarkMagician-SDY-NA-UR-UE.png",
                     "imageinfo":[{"url":"https://ms.yugipedia.com/abc/DarkMagician-SDY-NA-UR-UE.png"}]},
            "99999":{"title":"File:DarkMagician-SDY-EU-UR-UE.png",
                     "imageinfo":[{"url":"https://ms.yugipedia.com/eu/DarkMagician-SDY-EU-UR-UE.png"}]}
          }}
        })";
        const std::vector<std::string> order = {
            "DarkMagician-SDY-EN-UR-UE.png",   // missing from response entirely
            "DarkMagician-SDY-EN-UR-1E.png",   // present but `missing`
            "DarkMagician-SDY-NA-UR-UE.png",   // first existing one
            "DarkMagician-SDY-EU-UR-UE.png",
        };
        const auto out = YuGiOhCardPreviewSource::parseYugipediaResponse(body, order);
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://ms.yugipedia.com/abc/DarkMagician-SDY-NA-UR-UE.png");
    }

    TEST_CASE("pure-miss response is classified as NotFound") {
        // Pure-miss response: every page is marked `missing`. The caller
        // (fetchImageUrl) treats this as "no Yugipedia scan" and triggers
        // the YGOPRODeck fallback. Yugipedia having explicitly answered
        // "no such file" is a clean negative (the upstream contract was
        // honored), not a transient failure.
        const std::string body = R"({
          "query":{"pages":{
            "-1":{"title":"File:Whatever-XYZ-EN-UR-UE.png","missing":""},
            "-2":{"title":"File:Whatever-XYZ-NA-UR-UE.png","missing":""}
          }}
        })";
        const auto out = YuGiOhCardPreviewSource::parseYugipediaResponse(
            body, {"Whatever-XYZ-EN-UR-UE.png", "Whatever-XYZ-NA-UR-UE.png"});
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("malformed JSON is classified as Transient") {
        const auto out = YuGiOhCardPreviewSource::parseYugipediaResponse(
            "{not json", {"Anything-XYZ-EN-UR-UE.png"});
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("missing top-level query object is Transient") {
        const auto out = YuGiOhCardPreviewSource::parseYugipediaResponse(
            R"({"not_query":{}})", {"File.png"});
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("query.pages not an object is Transient") {
        const auto out = YuGiOhCardPreviewSource::parseYugipediaResponse(
            R"({"query":{"pages":[]}})", {"X.png"});
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::parseFallbackImageUrl") {
    TEST_CASE("missing data array is Transient") {
        const auto out = YuGiOhCardPreviewSource::parseFallbackImageUrl(R"({"meta":{}})", "Dark Magician");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("data present but not an array is Transient") {
        const auto out =
            YuGiOhCardPreviewSource::parseFallbackImageUrl(R"({"data":{}})", "X");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("empty data array is NotFound") {
        const auto out = YuGiOhCardPreviewSource::parseFallbackImageUrl(R"({"data":[]})", "X");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("prefers exact-name row with image_url_small when image_url absent") {
        const std::string json = R"({"data":[
          {"name":"Dark Magician Girl","card_images":[{"image_url_small":"https://small.only/a.jpg"}]},
          {"name":"Dark Magician","card_images":[{"image_url":"https://ignored/wrong.jpg"}]}
        ]})";
        const auto out = YuGiOhCardPreviewSource::parseFallbackImageUrl(json, "Dark Magician Girl");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://small.only/a.jpg");
    }

    TEST_CASE("exact-name match uses image_url_cropped when earlier slots absent") {
        const std::string json = R"({"data":[{
          "name":"Slifer",
          "card_images":[{"image_url_cropped":"https://crop/z.jpg"}]
        }]})";
        const auto out = YuGiOhCardPreviewSource::parseFallbackImageUrl(json, "Slifer");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://crop/z.jpg");
    }

    TEST_CASE("no exact name match falls back to first ranked card_images row") {
        const std::string json = R"({"data":[{
          "name":"Dark Magician Girl",
          "card_images":[{"image_url":"https://images/std-from-ranked-first.jpg"}]
        }]})";
        const auto out =
            YuGiOhCardPreviewSource::parseFallbackImageUrl(json, "Dark Magician");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images/std-from-ranked-first.jpg");
    }

    TEST_CASE("matching cards without usable images is NotFound") {
        const std::string json = R"({"data":[{
          "name":"Empty Card",
          "card_images":[{}]
        }]})";
        const auto out =
            YuGiOhCardPreviewSource::parseFallbackImageUrl(json, "Empty Card");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("malformed JSON is Transient") {
        const auto out =
            YuGiOhCardPreviewSource::parseFallbackImageUrl("{not json", "Any");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::parseFirstPrint") {
    TEST_CASE("returns first print for preferred set name") {
        const std::string json = R"({
          "data":[
            {"card_sets":[
              {"set_name":"Metal Raiders","set_code":"MRD-001","set_rarity":"Ultra Rare"},
              {"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB-001","set_rarity":"Ultra Rare"}
            ]}
          ]
        })";
        const auto out = YuGiOhCardPreviewSource::parseFirstPrint(
            json, "Legend of Blue Eyes White Dragon");
        REQUIRE(out.isOk());
        CHECK(out.value().setNo == "LOB-001");
        CHECK(out.value().rarity == "Ultra Rare");
    }

    TEST_CASE("empty data array yields error") {
        const auto out =
            YuGiOhCardPreviewSource::parseFirstPrint(R"({"data":[]})", "Any Set");
        CHECK(out.isErr());
    }

    TEST_CASE("card row without card_sets yields error") {
        const auto out = YuGiOhCardPreviewSource::parseFirstPrint(
            R"({"data":[{"name":"Solo"}]})", "Any Display Set");
        CHECK(out.isErr());
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::parsePrintVariants") {
    TEST_CASE("lists distinct set_code entries for an exact name in one display set") {
        const std::string json = R"({
          "data":[
            {"name":"Test Goblin",
             "card_sets":[
               {"set_name":"Mega Pack","set_code":"MP21-EN001","set_rarity":"Common"},
               {"set_name":"Mega Pack","set_code":"MP21-EN002","set_rarity":"Rare"}
             ]}
          ]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Mega Pack", "Test Goblin");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].setNo == "MP21-EN001");
        CHECK(out.value()[1].setNo == "MP21-EN002");
    }

    TEST_CASE("lists multiple rarities for one set_code") {
        const std::string json = R"({
          "data":[
            {"name":"Odd-Eyes Pendulum Dragon",
             "card_sets":[
               {"set_name":"Duelist Alliance","set_code":"DUEA-EN004","set_rarity":"Super Rare"},
               {"set_name":"Duelist Alliance","set_code":"DUEA-EN004","set_rarity":"Ultimate Rare"}
             ]}
          ]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Duelist Alliance", "Odd-Eyes Pendulum Dragon");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].setNo == "DUEA-EN004");
        CHECK(out.value()[0].rarity == "Super Rare");
        CHECK(out.value()[1].setNo == "DUEA-EN004");
        CHECK(out.value()[1].rarity == "Ultimate Rare");
    }

    TEST_CASE("dedupes identical print pairs") {
        const std::string json = R"({
          "data":[
            {"name":"Mirror Force",
             "card_sets":[
               {"set_name":"Starter Deck","set_code":"SDY-043","set_rarity":"Super Rare"},
               {"set_name":"Starter Deck","set_code":"SDY-043","set_rarity":"Super Rare"}
             ]}
          ]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Starter Deck", "Mirror Force");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "SDY-043");
        CHECK(out.value()[0].rarity == "Super Rare");
    }

    TEST_CASE("malformed JSON surfaces as parse error") {
        const auto out =
            YuGiOhCardPreviewSource::parsePrintVariants("{bad json", "Mega Pack", "X");
        REQUIRE(out.isErr());
        CHECK(out.error().find("YGOPRODeck JSON parse error") != std::string::npos);
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::detectPrintVariants HTTP fallback") {
    TEST_CASE("retries without cardset when the filtered request fails") {
        FailFilteredThenOkHttpClient http;
        http.unfilteredBody = R"({
          "data":[{
            "name":"Test Goblin",
            "card_sets":[
              {"set_name":"Mega Pack","set_code":"MP21-EN001","set_rarity":"Common"}
            ]
          }]
        })";

        YuGiOhCardPreviewSource src{http};
        const auto out = src.detectPrintVariants("Test Goblin", "Mega Pack");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "MP21-EN001");
        REQUIRE(http.calls == 2);
    }
}

// Helpers aligned with external fixture `yugioh_same_card_set_variant_tests`
// (same_card_same_set: distinct collector numbers vs distinct rarities).
[[nodiscard]] std::size_t countDistinctSetCodes(const std::vector<AutoDetectedPrint>& v) {
    std::unordered_set<std::string> codes;
    for (const auto& p : v) {
        if (!p.setNo.empty()) codes.insert(p.setNo);
    }
    return codes.size();
}

[[nodiscard]] std::size_t maxRarityVariantsPerCode(const std::vector<AutoDetectedPrint>& v) {
    std::unordered_map<std::string, std::unordered_set<std::string>> byCode;
    for (const auto& p : v) {
        if (p.setNo.empty() || p.rarity.empty()) continue;
        byCode[p.setNo].insert(p.rarity);
    }
    std::size_t mx = 0;
    for (const auto& e : byCode) {
        mx = std::max(mx, e.second.size());
    }
    return mx;
}

TEST_SUITE("YuGiOhCardPreviewSource::parsePrintVariants yugioh_same_card_set_variant_tests") {
    TEST_CASE("S1-001 Armed Dragon LV7 Soul of the Duelist Ultra vs Ultimate") {
        const std::string json = R"({
          "data":[{
            "name":"Armed Dragon LV7",
            "card_sets":[
              {"set_name":"Soul of the Duelist","set_code":"SOD-EN015","set_rarity":"Ultra Rare"},
              {"set_name":"Soul of the Duelist","set_code":"SOD-EN015","set_rarity":"Ultimate Rare"}
            ]
          }]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Soul of the Duelist", "Armed Dragon LV7");
        REQUIRE(out.isOk());
        CHECK(countDistinctSetCodes(out.value()) == 1);
        CHECK(maxRarityVariantsPerCode(out.value()) == 2);
    }

    TEST_CASE("S1-002 Horus LV8 Soul of the Duelist Ultra vs Ultimate") {
        const std::string json = R"({
          "data":[{
            "name":"Horus the Black Flame Dragon LV8",
            "card_sets":[
              {"set_name":"Soul of the Duelist","set_code":"SOD-EN008","set_rarity":"Ultra Rare"},
              {"set_name":"Soul of the Duelist","set_code":"SOD-EN008","set_rarity":"Ultimate Rare"}
            ]
          }]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Soul of the Duelist", "Horus the Black Flame Dragon LV8");
        REQUIRE(out.isOk());
        CHECK(countDistinctSetCodes(out.value()) == 1);
        CHECK(maxRarityVariantsPerCode(out.value()) == 2);
    }

    TEST_CASE("S1-003 Mobius Soul of the Duelist Super vs Ultimate") {
        const std::string json = R"({
          "data":[{
            "name":"Mobius the Frost Monarch",
            "card_sets":[
              {"set_name":"Soul of the Duelist","set_code":"SOD-EN022","set_rarity":"Super Rare"},
              {"set_name":"Soul of the Duelist","set_code":"SOD-EN022","set_rarity":"Ultimate Rare"}
            ]
          }]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Soul of the Duelist", "Mobius the Frost Monarch");
        REQUIRE(out.isOk());
        CHECK(countDistinctSetCodes(out.value()) == 1);
        CHECK(maxRarityVariantsPerCode(out.value()) == 2);
    }

    TEST_CASE("S2-001 Dark Magician Yugi's Legendary Decks three set codes") {
        const std::string json = R"({
          "data":[{
            "name":"Dark Magician",
            "card_sets":[
              {"set_name":"Yugi's Legendary Decks","set_code":"YGLD-ENA03","set_rarity":"Common"},
              {"set_name":"Yugi's Legendary Decks","set_code":"YGLD-ENB02","set_rarity":"Common"},
              {"set_name":"Yugi's Legendary Decks","set_code":"YGLD-ENC09","set_rarity":"Common"}
            ]
          }]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Yugi's Legendary Decks", "Dark Magician");
        REQUIRE(out.isOk());
        CHECK(countDistinctSetCodes(out.value()) == 3);
        CHECK(maxRarityVariantsPerCode(out.value()) == 1);
    }

    TEST_CASE("S2-002 Dark Magician Girl Yugi's Legendary Decks two set codes") {
        const std::string json = R"({
          "data":[{
            "name":"Dark Magician Girl",
            "card_sets":[
              {"set_name":"Yugi's Legendary Decks","set_code":"YGLD-ENA04","set_rarity":"Common"},
              {"set_name":"Yugi's Legendary Decks","set_code":"YGLD-ENC10","set_rarity":"Common"}
            ]
          }]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Yugi's Legendary Decks", "Dark Magician Girl");
        REQUIRE(out.isOk());
        CHECK(countDistinctSetCodes(out.value()) == 2);
        CHECK(maxRarityVariantsPerCode(out.value()) == 1);
    }

    TEST_CASE("NEG-001 duplicate rarity lines collapse to one variant") {
        const std::string json = R"({
          "data":[{
            "name":"Armed Dragon LV7",
            "card_sets":[
              {"set_name":"Soul of the Duelist","set_code":"SOD-EN015","set_rarity":"Ultra Rare"},
              {"set_name":"Soul of the Duelist","set_code":"SOD-EN015","set_rarity":"Ultra Rare"}
            ]
          }]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Soul of the Duelist", "Armed Dragon LV7");
        REQUIRE(out.isOk());
        CHECK(out.value().size() == 1);
        CHECK(maxRarityVariantsPerCode(out.value()) == 1);
    }

    TEST_CASE("NEG-002 duplicate set codes collapse to one collector slot") {
        const std::string json = R"({
          "data":[{
            "name":"Dark Magician",
            "card_sets":[
              {"set_name":"Yugi's Legendary Decks","set_code":"YGLD-ENA03","set_rarity":"Common"},
              {"set_name":"Yugi's Legendary Decks","set_code":"YGLD-ENA03","set_rarity":"Common"}
            ]
          }]
        })";
        const auto out = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Yugi's Legendary Decks", "Dark Magician");
        REQUIRE(out.isOk());
        CHECK(countDistinctSetCodes(out.value()) == 1);
        CHECK(maxRarityVariantsPerCode(out.value()) == 1);
    }

    TEST_CASE("NEG-003 no cross-product merge when display set matches nothing") {
        const std::string json = R"({
          "data":[{
            "name":"Dark Magician",
            "card_sets":[
              {"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB-005","set_rarity":"Ultra Rare"},
              {"set_name":"Starter Deck: Yugi","set_code":"SDY-006","set_rarity":"Ultra Rare"}
            ]
          }]
        })";
        const auto bogus = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Different Sets", "Dark Magician");
        CHECK(bogus.isErr());

        const auto lob = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Legend of Blue Eyes White Dragon", "Dark Magician");
        REQUIRE(lob.isOk());
        REQUIRE(lob.value().size() == 1);
        CHECK(lob.value()[0].setNo == "LOB-005");

        const auto sdy = YuGiOhCardPreviewSource::parsePrintVariants(
            json, "Starter Deck: Yugi", "Dark Magician");
        REQUIRE(sdy.isOk());
        REQUIRE(sdy.value().size() == 1);
        CHECK(sdy.value()[0].setNo == "SDY-006");
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::fetchImageUrl") {
    TEST_CASE("supports auto-detect print metadata") {
        FixedHttpClient http;
        YuGiOhCardPreviewSource src{http};
        CHECK(src.supportsAutoDetectPrint());
    }

    TEST_CASE("queries Yugipedia first and uses the per-printing scan when found") {
        // Two same-passcode reprints with genuinely different art (LOB vs
        // SDK Blue-Eyes). Yugipedia hosts both, so we should always pick the
        // SDK one for the SDK input - no leak of the LOB scan into the SDK
        // entry, which was the user-reported regression on YGOPRODeck.
        RoutingHttpClient http;
        http.yugipediaBody = R"({"query":{"pages":{
          "1":{"title":"File:BlueEyesWhiteDragon-SDK-NA-UR-UE.png",
               "imageinfo":[{"url":"https://ms.yugipedia.com/sdk/BlueEyesWhiteDragon-SDK-NA-UR-UE.png"}]}
        }}})";
        // Make the YGOPRODeck fallback obviously wrong so we can assert it
        // isn't being used when Yugipedia returns a hit.
        http.ygoprodeckBody = R"({"data":[{"name":"Blue-Eyes White Dragon",
          "card_images":[{"image_url":"https://example.invalid/wrong.png"}],
          "card_sets":[{"set_code":"SDK-001","set_rarity":"Ultra Rare"}]}]})";

        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl(
            "Blue-Eyes White Dragon", "Starter Deck: Kaiba", "SDK-001||Ultra Rare||UE");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://ms.yugipedia.com/sdk/BlueEyesWhiteDragon-SDK-NA-UR-UE.png");
        // Verified: only the Yugipedia call was made, not the YGOPRODeck one.
        REQUIRE(http.calls.size() == 1);
        CHECK(http.calls.front().find("yugipedia.com") != std::string::npos);
    }

    TEST_CASE("falls back to YGOPRODeck standard art when Yugipedia has no match") {
        // Empty `pages` -> Yugipedia parser returns an error -> fallback
        // path kicks in. This is the safety net for OCG-only or just-released
        // cards that don't yet have an English scan uploaded to Yugipedia.
        RoutingHttpClient http;
        http.yugipediaBody = R"({"query":{"pages":{
          "-1":{"title":"File:DarkMagician-LOB-EN-UR-UE.png","missing":""}
        }}})";
        http.ygoprodeckBody = R"({"data":[{"name":"Dark Magician",
          "card_images":[{"image_url":"https://images.ygoprodeck.com/std-dm.jpg"}],
          "card_sets":[{"set_code":"LOB-005","set_rarity":"Ultra Rare"}]}]})";

        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl(
            "Dark Magician", "Legend of Blue Eyes White Dragon", "LOB-005||Ultra Rare||UE");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images.ygoprodeck.com/std-dm.jpg");
        // Two HTTP calls: Yugipedia first, YGOPRODeck second.
        REQUIRE(http.calls.size() == 2);
        CHECK(http.calls[0].find("yugipedia.com") != std::string::npos);
        CHECK(http.calls[1].find("ygoprodeck.com") != std::string::npos);
    }

    TEST_CASE("falls back when Yugipedia errors out (transient HTTP failure)") {
        // We don't want a Yugipedia outage to leave the user without any
        // preview at all - YGOPRODeck's generic art is acceptable degraded
        // behavior.
        RoutingHttpClient http;
        http.yugipediaOk = false;
        http.ygoprodeckBody = R"({"data":[{"name":"Dark Magician",
          "card_images":[{"image_url":"https://images.ygoprodeck.com/std-dm.jpg"}]}]})";

        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl(
            "Dark Magician", "Legend of Blue Eyes White Dragon", "LOB-005||Ultra Rare||UE");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images.ygoprodeck.com/std-dm.jpg");
    }

    TEST_CASE("propagates the YGOPRODeck error as Transient when the fallback also fails") {
        // Both upstreams unreachable -> the overall outcome is transient.
        // CardPreviewService relies on this to avoid negative-caching when
        // the user's connection is offline.
        RoutingHttpClient http;
        http.yugipediaOk = false;
        http.ygoprodeckOk = false;

        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl(
            "Dark Magician", "Legend of Blue Eyes White Dragon", "LOB-005||Ultra Rare||UE");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("classifies as NotFound only when both upstreams answered cleanly with no match") {
        // Yugipedia: clean miss (every candidate "missing").
        // YGOPRODeck: clean miss (empty data array).
        // -> Both upstreams agree -> NotFound, safe to remember.
        RoutingHttpClient http;
        http.yugipediaBody = R"({"query":{"pages":{
          "-1":{"title":"File:Whatever-LOB-EN-UR-UE.png","missing":""}
        }}})";
        http.ygoprodeckBody = R"({"data":[]})";

        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl(
            "No Such Card", "Legend of Blue Eyes White Dragon", "LOB-999||Ultra Rare||UE");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("Yugipedia transient + YGOPRODeck clean-miss is overall Transient") {
        // We can't conclude the card has no image when one upstream
        // couldn't speak. This is the cautious path that keeps the offline
        // user out of the "permanent no-preview" trap.
        RoutingHttpClient http;
        http.yugipediaOk = false;  // network failure
        http.ygoprodeckBody = R"({"data":[]})";  // clean miss

        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl(
            "Possibly Real", "Some Set", "ABC-001||Ultra Rare||UE");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("skips Yugipedia entirely when the set code is missing") {
        // Without a set code we can't construct any candidate filename - go
        // straight to the YGOPRODeck fallback to avoid wasting an HTTP call.
        FixedHttpClient http;
        http.ok = true;
        http.body = R"({"data":[{"name":"Dark Magician",
          "card_images":[{"image_url":"https://images.ygoprodeck.com/std-dm.jpg"}]}]})";

        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl(
            "Dark Magician", "Legend of Blue Eyes White Dragon", "");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images.ygoprodeck.com/std-dm.jpg");
        CHECK(http.lastUrl.find("ygoprodeck.com") != std::string::npos);
        CHECK(http.lastUrl.find("yugipedia.com") == std::string::npos);
    }
}
