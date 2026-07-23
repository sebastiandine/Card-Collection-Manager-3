#include <doctest/doctest.h>

#include "ccm/games/pokemon/PokemonSetSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <unordered_map>

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

class RoutingHttpClient final : public IHttpClient {
public:
    std::string listBody;
    std::unordered_map<std::string, std::string> byUrl;
    std::string lastUrl;
    Result<std::string> get(std::string_view url) override {
        lastUrl = std::string(url);
        if (lastUrl == PokemonSetSource::kListEndpoint) {
            return Result<std::string>::ok(listBody);
        }
        const auto it = byUrl.find(lastUrl);
        if (it == byUrl.end()) return Result<std::string>::err("missing route");
        return Result<std::string>::ok(it->second);
    }
};

}  // namespace

TEST_SUITE("PokemonSetSource::parseListResponse") {
    TEST_CASE("happy path: maps id/name from top-level array") {
        const std::string json = R"([
            {"id":"base1","name":"Base Set","cardCount":{"total":102,"official":102}},
            {"id":"base2","name":"Jungle","cardCount":{"total":64,"official":64}}
        ])";

        const auto out = PokemonSetSource::parseListResponse(json);
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].id == "base1");
        CHECK(out.value()[0].name == "Base Set");
        CHECK(out.value()[0].releaseDate.empty());
        CHECK(out.value()[1].id == "base2");
    }

    TEST_CASE("empty array returns an empty list (not an error)") {
        const auto out = PokemonSetSource::parseListResponse("[]");
        REQUIRE(out.isOk());
        CHECK(out.value().empty());
    }

    TEST_CASE("object shape returns an error") {
        const auto out = PokemonSetSource::parseListResponse(R"({"data":[]})");
        CHECK(out.isErr());
    }

    TEST_CASE("invalid JSON returns an error") {
        const auto out = PokemonSetSource::parseListResponse("{not json");
        CHECK(out.isErr());
    }
}

TEST_SUITE("PokemonSetSource::parseReleaseDate") {
    TEST_CASE("rewrites YYYY-MM-DD to YYYY/MM/DD") {
        const auto out = PokemonSetSource::parseReleaseDate(
            R"({"id":"base1","releaseDate":"1999-01-09"})");
        REQUIRE(out.isOk());
        CHECK(out.value() == "1999/01/09");
    }

    TEST_CASE("missing releaseDate yields empty string") {
        const auto out = PokemonSetSource::parseReleaseDate(R"({"id":"base1"})");
        REQUIRE(out.isOk());
        CHECK(out.value().empty());
    }
}

TEST_SUITE("PokemonSetSource::parseCatalogPackFromSetDetail") {
    TEST_CASE("builds checklist from cards localId/name and dedupes") {
        const Set set{"base1", "Base Set", "1999/01/09"};
        const std::string json = R"({
            "id":"base1",
            "name":"Base Set",
            "cards":[
                {"id":"base1-4","localId":"4","name":"Charizard"},
                {"id":"base1-4","localId":"4/102","name":"Charizard"},
                {"id":"base1-58","localId":"58","name":"Growlithe"}
            ]
        })";
        const auto pack = PokemonSetSource::parseCatalogPackFromSetDetail(json, set);
        REQUIRE(pack.isOk());
        CHECK(pack.value().setId == "base1");
        REQUIRE(pack.value().cards.size() == 2);
        CHECK(pack.value().cards[0].setNo == "4");
        CHECK(pack.value().cards[1].setNo == "58");
    }
}

TEST_SUITE("PokemonSetSource::fetchAll") {
    TEST_CASE("network error is surfaced as a Result error") {
        FixedHttpClient http;
        http.ok = false;
        PokemonSetSource src{http};
        CHECK(src.fetchAll().isErr());
    }

    TEST_CASE("list plus set detail fills release dates and hits EN endpoints") {
        RoutingHttpClient http;
        http.listBody = R"([{"id":"base1","name":"Base Set"}])";
        http.byUrl[PokemonSetSource::buildSetDetailUrl("base1")] =
            R"({"id":"base1","name":"Base Set","releaseDate":"1999-01-09","cards":[]})";
        PokemonSetSource src{http};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value().front().id == "base1");
        CHECK(out.value().front().releaseDate == "1999/01/09");
        CHECK(http.lastUrl == PokemonSetSource::buildSetDetailUrl("base1"));
    }
}

TEST_SUITE("PokemonSetSource::fetchAllWithCatalog") {
    TEST_CASE("builds catalog packs from set detail cards") {
        RoutingHttpClient http;
        http.listBody = R"([{"id":"base1","name":"Base Set"}])";
        http.byUrl[PokemonSetSource::buildSetDetailUrl("base1")] = R"({
            "id":"base1",
            "name":"Base Set",
            "releaseDate":"1999-01-09",
            "cards":[
                {"localId":"4","name":"Charizard"},
                {"localId":"58","name":"Growlithe"}
            ]
        })";
        PokemonSetSource src{http};
        const auto out = src.fetchAllWithCatalog();
        REQUIRE(out.isOk());
        REQUIRE(out.value().sets.size() == 1);
        REQUIRE(out.value().catalog.packs.size() == 1);
        const auto* pack = out.value().catalog.findPack("base1");
        REQUIRE(pack != nullptr);
        REQUIRE(pack->cards.size() == 2);
        CHECK(pack->cards[0].setNo == "4");
    }
}
