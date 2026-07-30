#include "ccm/games/yugiohbandai/YuGiOhBandaiSetSource.hpp"

#include <doctest/doctest.h>

#include <string>

using namespace ccm;

namespace {

class FixedHttpClient final : public IHttpClient {
public:
    std::string body;
    std::string lastUrl;
    bool        fail{false};

    Result<std::string> get(std::string_view url) override {
        lastUrl = std::string(url);
        if (fail) return Result<std::string>::err("http fail");
        return Result<std::string>::ok(body);
    }
};

}  // namespace

TEST_SUITE("YuGiOhBandaiSetSource") {
    TEST_CASE("parseResponse returns stable manifest ordered by release date") {
        auto sets = YuGiOhBandaiSetSource::parseResponse({});
        REQUIRE(sets);
        REQUIRE(sets.value().size() == 6);
        CHECK(sets.value()[0].id == "ban1");
        CHECK(sets.value()[0].name == "1st Generation");
        CHECK(sets.value()[0].releaseDate == "1998/09/01");
        CHECK(sets.value()[5].id == "bansealdass");
    }

    TEST_CASE("normalizeCardNumber strips leading zeros and uppercases prefixes") {
        CHECK(YuGiOhBandaiSetSource::normalizeCardNumber("014") == "14");
        CHECK(YuGiOhBandaiSetSource::normalizeCardNumber("#9") == "9");
        CHECK(YuGiOhBandaiSetSource::normalizeCardNumber("j1") == "J1");
        CHECK(YuGiOhBandaiSetSource::normalizeCardNumber("ta2") == "TA2");
        CHECK(YuGiOhBandaiSetSource::normalizeCardNumber("  ") == "");
    }

    TEST_CASE("expandRarityCode maps gallery abbreviations") {
        CHECK(YuGiOhBandaiSetSource::expandRarityCode("C") == "Common");
        CHECK(YuGiOhBandaiSetSource::expandRarityCode("R") == "Rare");
        CHECK(YuGiOhBandaiSetSource::expandRarityCode("SR") == "Super Rare");
        CHECK(YuGiOhBandaiSetSource::expandRarityCode("HFR") == "Holo Seal");
    }

    TEST_CASE("setIdForNumber maps ranges and promo prefixes") {
        CHECK(YuGiOhBandaiSetSource::setIdForNumber("14") == "ban1");
        CHECK(YuGiOhBandaiSetSource::setIdForNumber("50") == "ban2");
        CHECK(YuGiOhBandaiSetSource::setIdForNumber("118") == "ban3");
        CHECK(YuGiOhBandaiSetSource::setIdForNumber("J1") == "banpromo-j");
        CHECK(YuGiOhBandaiSetSource::setIdForNumber("TA2") == "banpromo-ta");
    }

    TEST_CASE("parseGalleryWikitext extracts number rarity and English name") {
        const std::string wiki =
            "DarkMagician-BAN1-JP-R.png | {{pound}}014 ([[R]]) "
            "{{Gallery card names|Dark Magician (Bandai)|ja}}\n"
            "BlueEyesWhiteDragon-BAN1-JP-SR.png | {{pound}}009 ([[SR]]) "
            "{{Gallery card names|Blue-Eyes White Dragon (Bandai)|ja}}\n";

        auto cards = YuGiOhBandaiSetSource::parseGalleryWikitext(wiki);
        REQUIRE(cards);
        REQUIRE(cards.value().size() == 2);
        CHECK(cards.value()[0].setNo == "14");
        CHECK(cards.value()[0].name == "Dark Magician");
        CHECK(cards.value()[0].rarity == "Rare");
        CHECK(cards.value()[1].setNo == "9");
        CHECK(cards.value()[1].rarity == "Super Rare");
    }

    TEST_CASE("parseGalleryWikitext accepts promo [[TA2]] number format") {
        const std::string wiki =
            "WickedChain-BAN1-JP-SR.png | [[TA1]] ([[SR]]) "
            "{{Gallery card names|Wicked Chain|ja}}\n"
            "BlueEyesWhiteDragons3BodyConnection-BAN1-JP-SR.png | [[TA2]] ([[SR]]) "
            "{{Gallery card names|Blue-Eyes White Dragon's 3-Body Connection|ja}}\n"
            "MirrorForce-BAN1-JP-SR.png | [[J1]] ([[SR]]) "
            "{{Gallery card names|Mirror Force (Bandai)|ja}}\n";

        auto cards = YuGiOhBandaiSetSource::parseGalleryWikitext(wiki);
        REQUIRE(cards);
        REQUIRE(cards.value().size() == 3);
        CHECK(cards.value()[0].setNo == "TA1");
        CHECK(cards.value()[0].name == "Wicked Chain");
        CHECK(cards.value()[0].rarity == "Super Rare");
        CHECK(cards.value()[1].setNo == "TA2");
        CHECK(cards.value()[1].name == "Blue-Eyes White Dragon's 3-Body Connection");
        CHECK(cards.value()[2].setNo == "J1");
        CHECK(cards.value()[2].name == "Mirror Force");
    }

    TEST_CASE("parseGalleryWikitext tolerates <br /> between rarity and name template") {
        // Live Yugipedia promo gallery captions insert <br /> after expansion.
        const std::string wiki =
            "<gallery mode=\"packed\">\n"
            "BlueEyesWhiteDragons3BodyConnection-BAN1-JP-SR.png | [[TA2]] ([[SR]])<br />"
            "{{Gallery card names|Blue-Eyes White Dragon's 3-Body Connection|ja}}\n"
            "MirrorForce-BAN1-JP-SR.png | [[J1]] ([[SR]])<br />"
            "{{Gallery card names|Mirror Force (Bandai)|ja}}\n"
            "</gallery>\n";

        auto cards = YuGiOhBandaiSetSource::parseGalleryWikitext(wiki);
        REQUIRE(cards);
        REQUIRE(cards.value().size() == 2);
        CHECK(cards.value()[0].setNo == "TA2");
        CHECK(cards.value()[0].name == "Blue-Eyes White Dragon's 3-Body Connection");
        CHECK(cards.value()[0].rarity == "Super Rare");
        CHECK(cards.value()[1].setNo == "J1");
    }

    TEST_CASE("parseGalleryWikitext empty body yields empty ok") {
        auto cards = YuGiOhBandaiSetSource::parseGalleryWikitext("");
        REQUIRE(cards);
        CHECK(cards.value().empty());
    }

    TEST_CASE("fetchAll returns manifest without HTTP") {
        FixedHttpClient http;
        YuGiOhBandaiSetSource src(http);
        auto sets = src.fetchAll();
        REQUIRE(sets);
        CHECK(sets.value().size() == 6);
        CHECK(http.lastUrl.empty());
    }

    TEST_CASE("buildGalleryParseUrl percent-encodes page title") {
        const auto url = YuGiOhBandaiSetSource::buildGalleryParseUrl(
            "Set Card Galleries:Yu-Gi-Oh! Bandai OCG: 1st Generation");
        CHECK(url.find("action=parse") != std::string::npos);
        CHECK(url.find("prop=wikitext") != std::string::npos);
        CHECK(url.find("page=") != std::string::npos);
    }
}
