#include <doctest/doctest.h>

#include "ccm/games/yugioh/YuGiOhCardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>

using namespace ccm;

namespace {

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

}  // namespace

TEST_SUITE("YuGiOhCardPreviewSource::buildSearchUrl") {
    TEST_CASE("builds encoded endpoint with name and set name") {
        const auto url = YuGiOhCardPreviewSource::buildSearchUrl(
            "Dark Magician", "Legend of Blue Eyes White Dragon");
        CHECK(url.find("https://db.ygoprodeck.com/api/v7/cardinfo.php?fname=") == 0);
        CHECK(url.find("Dark%20Magician") != std::string::npos);
        CHECK(url.find("cardset=Legend%20of%20Blue%20Eyes%20White%20Dragon") != std::string::npos);
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::parseResponse") {
    TEST_CASE("picks exact set-code + rarity match first") {
        const std::string json = R"({
          "data":[
            {"card_images":[{"image_url":"https://img/a.jpg"}],
             "card_sets":[{"set_code":"LOB-005","set_rarity":"Ultra Rare"}]}
          ]
        })";
        const auto out = YuGiOhCardPreviewSource::parseResponse(json, "LOB-005", "Ultra Rare");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://img/a.jpg");
    }
}

TEST_SUITE("YuGiOhCardPreviewSource::fetchImageUrl") {
    TEST_CASE("network success parses with tuple-encoded setNo+rarity") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"({"data":[{"card_images":[{"image_url":"https://img/ok.jpg"}]}]})";
        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Dark Magician", "Legend of Blue Eyes White Dragon", "LOB-005||Ultra Rare");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://img/ok.jpg");
        CHECK(http.lastUrl.find("cardset=Legend%20of%20Blue%20Eyes%20White%20Dragon") != std::string::npos);
    }

    TEST_CASE("retries without set filter when first request fails") {
        class RetryHttpClient final : public IHttpClient {
        public:
            int calls{0};
            std::string lastUrl;
            Result<std::string> get(std::string_view url) override {
                ++calls;
                lastUrl = std::string(url);
                if (calls == 1) return Result<std::string>::err("HTTP 400");
                return Result<std::string>::ok(
                    R"({"data":[{"card_images":[{"image_url":"https://img/fallback.jpg"}]}]})");
            }
        } http;

        YuGiOhCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Blue Eyes White Dragon", "Legend of Blue Eyes White Dragon", "LOB-001||Ultra Rare");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://img/fallback.jpg");
        CHECK(http.calls == 2);
        CHECK(http.lastUrl.find("cardset=") == std::string::npos);
    }
}
