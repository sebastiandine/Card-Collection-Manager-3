#include "ccm/games/yugiohbandai/YuGiOhBandaiCardPreviewSource.hpp"

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

TEST_SUITE("YuGiOhBandaiCardPreviewSource helpers") {
    TEST_CASE("preferredPageTitle picks Bandai / English / Sealdass") {
        CHECK(YuGiOhBandaiCardPreviewSource::preferredPageTitle("Dark Magician", "ban1", "14") ==
              "Dark Magician (Bandai)");
        CHECK(YuGiOhBandaiCardPreviewSource::preferredPageTitle("Blue-Eyes White Dragon", "ban3",
                                                                "118") ==
              "Blue-Eyes White Dragon (English Bandai)");
        CHECK(YuGiOhBandaiCardPreviewSource::preferredPageTitle("Dark Magician", "bansealdass",
                                                                "2") ==
              "Dark Magician (Bandai Sealdass)");
    }

    TEST_CASE("buildPageImagesUrl encodes spaces as underscores then percent") {
        const auto url =
            YuGiOhBandaiCardPreviewSource::buildPageImagesUrl("Dark Magician (Bandai)");
        CHECK(url.find("titles=Dark_Magician_%28Bandai%29") != std::string::npos);
    }

    TEST_CASE("buildAskByNameUrl includes English name constraint") {
        const auto url = YuGiOhBandaiCardPreviewSource::buildAskByNameUrl("Dark Magician");
        CHECK(url.find("action=ask") != std::string::npos);
        CHECK(url.find("query=") != std::string::npos);
    }

    TEST_CASE("parsePageImagesResponse returns original source") {
        const std::string body = R"JSON({
          "query": {
            "pages": {
              "1": {
                "title": "Dark Magician (Bandai)",
                "original": {"source": "https://ms.yugipedia.com/d/d0/DarkMagician.png"}
              }
            }
          }
        })JSON";
        auto out = YuGiOhBandaiCardPreviewSource::parsePageImagesResponse(body);
        REQUIRE(out);
        CHECK(out.value() == "https://ms.yugipedia.com/d/d0/DarkMagician.png");
    }

    TEST_CASE("parsePageImagesResponse missing page is NotFound") {
        const std::string body = R"JSON({
          "query": { "pages": { "-1": { "missing": true, "title": "Nope" } } }
        })JSON";
        auto out = YuGiOhBandaiCardPreviewSource::parsePageImagesResponse(body);
        REQUIRE_FALSE(out);
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("parseAskResponse fills setNo rarity name and setId") {
        const std::string body = R"JSON({
          "query": {
            "results": {
              "Dark Magician (Bandai)": {
                "printouts": {
                  "English name": ["Dark Magician"],
                  "Bandai number": [14],
                  "Rarity": [{"fulltext": "Rare"}]
                }
              }
            }
          }
        })JSON";
        auto out = YuGiOhBandaiCardPreviewSource::parseAskResponse(body, "ban1");
        REQUIRE(out);
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].name == "Dark Magician");
        CHECK(out.value()[0].setNo == "14");
        CHECK(out.value()[0].rarity == "Rare");
        CHECK(out.value()[0].setId == "ban1");
        CHECK(out.value()[0].language == "Japanese");
    }

    TEST_CASE("parseAskResponse prefers Bandai over Sealdass when set is ban1") {
        const std::string body = R"JSON({
          "query": {
            "results": {
              "Dark Magician (Bandai Sealdass)": {
                "printouts": {
                  "English name": ["Dark Magician"],
                  "Bandai number": [2],
                  "Rarity": [{"fulltext": "Common"}]
                }
              },
              "Dark Magician (Bandai)": {
                "printouts": {
                  "English name": ["Dark Magician"],
                  "Bandai number": [14],
                  "Rarity": [{"fulltext": "Rare"}]
                }
              }
            }
          }
        })JSON";
        auto out = YuGiOhBandaiCardPreviewSource::parseAskResponse(body, "ban1");
        REQUIRE(out);
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].setNo == "14");
        CHECK(out.value()[0].setId == "ban1");
    }

    TEST_CASE("fetchImageUrl uses pageimages URL") {
        FixedHttpClient http;
        http.body = R"JSON({
          "query": {
            "pages": {
              "1": {
                "title": "Dark Magician (Bandai)",
                "original": {"source": "https://ms.yugipedia.com/x.png"}
              }
            }
          }
        })JSON";
        YuGiOhBandaiCardPreviewSource src(http);
        auto out = src.fetchImageUrl("Dark Magician", "ban1", "14");
        REQUIRE(out);
        CHECK(out.value() == "https://ms.yugipedia.com/x.png");
        CHECK(http.lastUrl.find("pageimages") != std::string::npos);
    }

    TEST_CASE("detectFirstPrint uses ask response") {
        FixedHttpClient http;
        http.body = R"JSON({
          "query": {
            "results": {
              "Dark Magician (Bandai)": {
                "printouts": {
                  "English name": ["Dark Magician"],
                  "Bandai number": [14],
                  "Rarity": [{"fulltext": "Rare"}]
                }
              }
            }
          }
        })JSON";
        YuGiOhBandaiCardPreviewSource src(http);
        auto out = src.detectFirstPrint("Dark Magician", "ban1");
        REQUIRE(out);
        CHECK(out.value().setNo == "14");
        CHECK(out.value().rarity == "Rare");
        CHECK(out.value().setId == "ban1");
    }

    TEST_CASE("detectBySetNo uses ask-by-number URL") {
        FixedHttpClient http;
        http.body = R"JSON({
          "query": {
            "results": {
              "Dark Magician (Bandai)": {
                "printouts": {
                  "English name": ["Dark Magician"],
                  "Bandai number": [14],
                  "Rarity": [{"fulltext": "Rare"}]
                }
              }
            }
          }
        })JSON";
        YuGiOhBandaiCardPreviewSource src(http);
        auto out = src.detectBySetNo("014");
        REQUIRE(out);
        CHECK(out.value().name == "Dark Magician");
        CHECK(http.lastUrl.find("action=ask") != std::string::npos);
    }

    TEST_CASE("detectBySetNo resolves promo TA2 from gallery parse") {
        FixedHttpClient http;
        http.body = R"JSON({
          "parse": {
            "wikitext": "WickedChain-BAN1-JP-SR.png | [[TA1]] ([[SR]]) {{Gallery card names|Wicked Chain|ja}}\nBlueEyesWhiteDragons3BodyConnection-BAN1-JP-SR.png | [[TA2]] ([[SR]])<br />{{Gallery card names|Blue-Eyes White Dragon's 3-Body Connection|ja}}\n"
          }
        })JSON";
        YuGiOhBandaiCardPreviewSource src(http);
        auto out = src.detectBySetNo("ta2");
        REQUIRE(out);
        CHECK(out.value().name == "Blue-Eyes White Dragon's 3-Body Connection");
        CHECK(out.value().setNo == "TA2");
        CHECK(out.value().setId == "banpromo-ta");
        CHECK(out.value().rarity == "Super Rare");
        CHECK(http.lastUrl.find("action=parse") != std::string::npos);
        CHECK(http.lastUrl.find("Promotional") != std::string::npos);
    }

    TEST_CASE("isAlphanumericPromoNumber detects Jump and Toei codes") {
        CHECK(YuGiOhBandaiCardPreviewSource::isAlphanumericPromoNumber("TA2"));
        CHECK(YuGiOhBandaiCardPreviewSource::isAlphanumericPromoNumber("j1"));
        CHECK_FALSE(YuGiOhBandaiCardPreviewSource::isAlphanumericPromoNumber("14"));
    }

    TEST_CASE("preferredPageTitle omits Bandai suffix for promo sets") {
        CHECK(YuGiOhBandaiCardPreviewSource::preferredPageTitle(
                  "Blue-Eyes White Dragon's 3-Body Connection", "banpromo-ta", "TA2") ==
              "Blue-Eyes White Dragon's 3-Body Connection");
    }
}
