#include <doctest/doctest.h>

#include "ccm/games/yugioh/YuGiOhCardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
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

}  // namespace

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
}

TEST_SUITE("YuGiOhCardPreviewSource::fetchImageUrl") {
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
