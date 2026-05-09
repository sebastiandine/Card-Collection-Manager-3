#include <doctest/doctest.h>

#include "ccm/games/yugioh/YuGiOhSetSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

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

TEST_SUITE("YuGiOhSetSource::parseResponse") {
    TEST_CASE("maps set_code/set_name/tcg_date") {
        const std::string json = R"([
            {"set_name":"Set A","set_code":"AAA","tcg_date":"2020-01-01"},
            {"set_name":"Set B","set_code":"BBB","tcg_date":"2021-02-03"}
        ])";
        const auto out = YuGiOhSetSource::parseResponse(json);
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].id == "AAA");
        CHECK(out.value()[0].releaseDate == "2020/01/01");
    }

    TEST_CASE("sorts by release date ascending") {
        const auto out = YuGiOhSetSource::parseResponse(R"([
            {"set_name":"New","set_code":"N","tcg_date":"2024-01-01"},
            {"set_name":"Old","set_code":"O","tcg_date":"2010-01-01"}
        ])");
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "O");
        CHECK(out.value().back().id == "N");
    }

    TEST_CASE("missing array returns error") {
        CHECK(YuGiOhSetSource::parseResponse(R"({"data":[]})").isErr());
    }
}

TEST_SUITE("YuGiOhSetSource::fetchAll") {
    TEST_CASE("network success parses and hits endpoint") {
        FixedHttpClient http;
        http.body = R"([{"set_name":"Set X","set_code":"X","tcg_date":"2020-01-01"}])";
        YuGiOhSetSource src{http};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "X");
        CHECK(http.lastUrl == "https://db.ygoprodeck.com/api/v7/cardsets.php");
    }
}
