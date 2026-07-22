#include <doctest/doctest.h>

#include "ccm/games/pokemonjp/JapanesePokemonCardPreviewSource.hpp"
#include "ccm/games/pokemonjp/JapanesePokemonEnCatalog.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <unordered_map>

using namespace ccm;

namespace {

class RoutingHttpClient final : public IHttpClient {
public:
    std::unordered_map<std::string, std::string> bodies;
    std::string lastUrl;
    bool ok = true;
    Result<std::string> get(std::string_view url) override {
        lastUrl = std::string(url);
        if (!ok) return Result<std::string>::err("offline");
        const auto it = bodies.find(lastUrl);
        if (it == bodies.end()) return Result<std::string>::err("unknown url: " + lastUrl);
        return Result<std::string>::ok(it->second);
    }
};

JapanesePokemonEnCatalog sampleCatalog() {
    auto c = JapanesePokemonEnCatalog::parse(R"({
        "sets": {
            "SV1a": {"name_en":"Triplet Beat","name_ja":"トリプレットビート"}
        },
        "prints": [
            {"set_id":"SV1a","local_id":"001","name_en":"Tropius","name_ja":"トロピウス","name_en_source":"bulbapedia"}
        ]
    })");
    REQUIRE(c.isOk());
    return std::move(c).value();
}

}  // namespace

TEST_SUITE("JapanesePokemonCardPreviewSource helpers") {
    TEST_CASE("normalizeLocalId strips slash and whitespace") {
        CHECK(JapanesePokemonCardPreviewSource::normalizeLocalId(" 001/102 ") == "001");
        CHECK(JapanesePokemonCardPreviewSource::normalizeLocalId("4/102") == "4");
    }

    TEST_CASE("imageUrlFromBase appends high.png") {
        CHECK(JapanesePokemonCardPreviewSource::imageUrlFromBase(
                  "https://assets.tcgdex.net/ja/SV/SV1a/001") ==
              "https://assets.tcgdex.net/ja/SV/SV1a/001/high.png");
    }

    TEST_CASE("buildCardUrl encodes set-local id") {
        CHECK(JapanesePokemonCardPreviewSource::buildCardUrl("SV1a", "001") ==
              "https://api.tcgdex.net/v2/ja/cards/SV1a-001");
    }
}

TEST_SUITE("JapanesePokemonCardPreviewSource::parseSetCards") {
    TEST_CASE("parses localId name and image") {
        const std::string json = R"({
            "id":"SV1a",
            "cards":[
                {"id":"SV1a-001","localId":"001","name":"トロピウス",
                 "image":"https://assets.tcgdex.net/ja/SV/SV1a/001","rarity":"Common"}
            ]
        })";
        const auto out = JapanesePokemonCardPreviewSource::parseSetCards(json);
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].localId == "001");
        CHECK(out.value()[0].nameJa == "トロピウス");
        CHECK(out.value()[0].imageBase == "https://assets.tcgdex.net/ja/SV/SV1a/001");
    }

    TEST_CASE("missing cards array is Transient") {
        const auto out = JapanesePokemonCardPreviewSource::parseSetCards(R"({"id":"X"})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("JapanesePokemonCardPreviewSource::parseCardImageUrl") {
    TEST_CASE("returns high.png URL") {
        const auto out = JapanesePokemonCardPreviewSource::parseCardImageUrl(
            R"({"id":"SV1a-001","image":"https://assets.tcgdex.net/ja/SV/SV1a/001"})");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://assets.tcgdex.net/ja/SV/SV1a/001/high.png");
    }

    TEST_CASE("null image is NotFound") {
        const auto out = JapanesePokemonCardPreviewSource::parseCardImageUrl(
            R"({"id":"PMCG1-001","image":null})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("malformed JSON is Transient") {
        const auto out = JapanesePokemonCardPreviewSource::parseCardImageUrl("{bad");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("JapanesePokemonCardPreviewSource::parsePrintVariants") {
    TEST_CASE("matches English catalog name") {
        const std::string body = R"({
            "id":"SV1a",
            "cards":[
                {"localId":"001","name":"トロピウス","rarity":"Common"},
                {"localId":"002","name":"other","rarity":"Common"}
            ]
        })";
        const auto catalog = sampleCatalog();
        const auto out = JapanesePokemonCardPreviewSource::parsePrintVariants(
            body, "SV1a", "Tropius", catalog);
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "001");
    }

    TEST_CASE("matches Japanese name without catalog") {
        const std::string body = R"({
            "id":"SV1a",
            "cards":[{"localId":"001","name":"トロピウス"}]
        })";
        JapanesePokemonEnCatalog empty;
        const auto out = JapanesePokemonCardPreviewSource::parsePrintVariants(
            body, "SV1a", "トロピウス", empty);
        REQUIRE(out.isOk());
        CHECK(out.value().front().setNo == "001");
    }

    TEST_CASE("rejects stale catalog localId when name_ja disagrees with TCGdex") {
        // Historical seed bug: Charmander mapped to 001 (actually Bulbasaur).
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"PMCG1","local_id":"001","name_en":"Charmander","name_ja":"ヒトカゲ"}
            ]
        })");
        REQUIRE(catalog.isOk());
        const std::string body = R"({
            "id":"PMCG1",
            "cards":[
                {"localId":"001","name":"フシギダネ","rarity":"Common"},
                {"localId":"014","name":"ヒトカゲ","rarity":"Common"}
            ]
        })";
        const auto out = JapanesePokemonCardPreviewSource::parsePrintVariants(
            body, "PMCG1", "Charmander", catalog.value());
        REQUIRE(out.isErr());
    }

    TEST_CASE("accepts corrected catalog localId for Charmander") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"PMCG1","local_id":"014","name_en":"Charmander","name_ja":"ヒトカゲ"}
            ]
        })");
        REQUIRE(catalog.isOk());
        const std::string body = R"({
            "id":"PMCG1",
            "cards":[
                {"localId":"001","name":"フシギダネ"},
                {"localId":"014","name":"ヒトカゲ","rarity":"Common"}
            ]
        })";
        const auto out = JapanesePokemonCardPreviewSource::parsePrintVariants(
            body, "PMCG1", "Charmander", catalog.value());
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "014");
    }

    TEST_CASE("English Blastoise and Mewtwo resolve Expansion Pack localIds") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"PMCG1","local_id":"032","name_en":"Blastoise","name_ja":"カメックス"},
                {"set_id":"PMCG1","local_id":"050","name_en":"Mewtwo","name_ja":"ミュウツー"}
            ]
        })");
        REQUIRE(catalog.isOk());
        const std::string body = R"({
            "id":"PMCG1",
            "cards":[
                {"localId":"032","name":"カメックス","rarity":"Holo Rare"},
                {"localId":"050","name":"ミュウツー","rarity":"Holo Rare"}
            ]
        })";
        auto blast = JapanesePokemonCardPreviewSource::parsePrintVariants(
            body, "PMCG1", "Blastoise", catalog.value());
        REQUIRE(blast.isOk());
        REQUIRE(blast.value().size() == 1);
        CHECK(blast.value()[0].setNo == "032");

        auto mew = JapanesePokemonCardPreviewSource::parsePrintVariants(
            body, "PMCG1", "Mewtwo", catalog.value());
        REQUIRE(mew.isOk());
        REQUIRE(mew.value().size() == 1);
        CHECK(mew.value()[0].setNo == "050");
    }

    TEST_CASE("English Switch resolves Expansion Pack localId 073") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"PMCG1","local_id":"073","name_en":"Switch","name_ja":"ポケモンいれかえ"}
            ]
        })");
        REQUIRE(catalog.isOk());
        const std::string body = R"({
            "id":"PMCG1",
            "cards":[
                {"localId":"071","name":"きずぐすり","rarity":"Common"},
                {"localId":"073","name":"ポケモンいれかえ","rarity":"Common"}
            ]
        })";
        const auto out = JapanesePokemonCardPreviewSource::parsePrintVariants(
            body, "PMCG1", "Switch", catalog.value());
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "073");
    }
}

TEST_SUITE("JapanesePokemonCardPreviewSource::fetchImageUrl") {
    TEST_CASE("resolves via card endpoint when localId present") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonCardPreviewSource::buildCardUrl("SV1a", "001")] =
            R"({"id":"SV1a-001","image":"https://assets.tcgdex.net/ja/SV/SV1a/001"})";
        auto catalog = sampleCatalog();
        JapanesePokemonCardPreviewSource src{http, catalog};
        const auto out = src.fetchImageUrl("Tropius", "SV1a", "001");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://assets.tcgdex.net/ja/SV/SV1a/001/high.png");
    }

    TEST_CASE("resolves via set detail when card has no image but set row does") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonCardPreviewSource::buildCardUrl("SV1a", "001")] =
            R"({"id":"SV1a-001","image":null})";
        http.bodies[JapanesePokemonCardPreviewSource::buildSetDetailUrl("SV1a")] = R"({
            "id":"SV1a",
            "cards":[{"localId":"001","name":"トロピウス",
                      "image":"https://assets.tcgdex.net/ja/SV/SV1a/001"}]
        })";
        auto catalog = sampleCatalog();
        JapanesePokemonCardPreviewSource src{http, catalog};
        const auto out = src.fetchImageUrl("Tropius", "SV1a", "001");
        REQUIRE(out.isOk());
        CHECK(out.value().find("/high.png") != std::string::npos);
    }

    TEST_CASE("stale catalog does not bind English name to wrong localId image") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonCardPreviewSource::buildSetDetailUrl("PMCG1")] = R"({
            "id":"PMCG1",
            "cards":[
                {"localId":"001","name":"フシギダネ",
                 "image":"https://assets.tcgdex.net/ja/PMCG/PMCG1/001"},
                {"localId":"014","name":"ヒトカゲ",
                 "image":"https://assets.tcgdex.net/ja/PMCG/PMCG1/014"}
            ]
        })";
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"PMCG1","local_id":"001","name_en":"Charmander","name_ja":"ヒトカゲ"}
            ]
        })");
        REQUIRE(catalog.isOk());
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        // Empty setNo forces name match; stale catalog must not pick Bulbasaur's art.
        const auto out = src.fetchImageUrl("Charmander", "PMCG1", "");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("English name resolves correct localId image via catalog") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonCardPreviewSource::buildSetDetailUrl("PMCG1")] = R"({
            "id":"PMCG1",
            "cards":[
                {"localId":"001","name":"フシギダネ",
                 "image":"https://assets.tcgdex.net/ja/PMCG/PMCG1/001"},
                {"localId":"014","name":"ヒトカゲ",
                 "image":"https://assets.tcgdex.net/ja/PMCG/PMCG1/014"}
            ]
        })";
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"PMCG1","local_id":"014","name_en":"Charmander","name_ja":"ヒトカゲ"}
            ]
        })");
        REQUIRE(catalog.isOk());
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        const auto out = src.fetchImageUrl("Charmander", "PMCG1", "");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://assets.tcgdex.net/ja/PMCG/PMCG1/014/high.png");
    }

    TEST_CASE("null image on set-specific card is NotFound without other-printing fallback") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonCardPreviewSource::buildCardUrl("PMCG1", "021")] =
            R"({"id":"PMCG1-021","name":"リザードン","image":null})";
        http.bodies[JapanesePokemonCardPreviewSource::buildSetDetailUrl("PMCG1")] = R"({
            "id":"PMCG1",
            "cards":[{"localId":"021","name":"リザードン"}]
        })";
        JapanesePokemonEnCatalog empty;
        JapanesePokemonCardPreviewSource src{http, empty};
        const auto out = src.fetchImageUrl("Charizard", "PMCG1", "021");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("catalog tcgplayer_id gap-fills when TCGdex image is null") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonCardPreviewSource::buildCardUrl("PMCG1", "021")] =
            R"({"id":"PMCG1-021","name":"リザードン","image":null})";
        http.bodies[JapanesePokemonCardPreviewSource::buildSetDetailUrl("PMCG1")] = R"({
            "id":"PMCG1",
            "cards":[{"localId":"021","name":"リザードン"}]
        })";
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"PMCG1","local_id":"021","name_en":"Charizard",
                 "name_ja":"リザードン","tcgplayer_id":"575604"}
            ]
        })");
        REQUIRE(catalog.isOk());
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        const auto out = src.fetchImageUrl("Charizard", "PMCG1", "021");
        REQUIRE(out.isOk());
        CHECK(out.value() ==
              "https://product-images.tcgplayer.com/fit-in/437x437/575604.jpg");
    }

    TEST_CASE("HTTP failure is Transient") {
        RoutingHttpClient http;
        http.ok = false;
        JapanesePokemonEnCatalog empty;
        JapanesePokemonCardPreviewSource src{http, empty};
        const auto out = src.fetchImageUrl("Tropius", "SV1a", "001");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("catalog-only theme deck resolves preview from tcgplayer_id") {
        RoutingHttpClient http;
        // No TCGdex bodies: card + set detail both miss.
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {"TamamushiCG":{"name_en":"Tamamushi City Gym"}},
            "prints": [
                {"set_id":"TamamushiCG","local_id":"021","name_en":"Celadon City Gym",
                 "name_ja":"タマムシシティジム","tcgplayer_id":"12345"}
            ]
        })");
        REQUIRE(catalog.isOk());
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        const auto out = src.fetchImageUrl("Celadon City Gym", "TamamushiCG", "021");
        REQUIRE(out.isOk());
        CHECK(out.value() ==
              "https://product-images.tcgplayer.com/fit-in/437x437/12345.jpg");
    }

    TEST_CASE("Tamamushi City Gym Erika uses catalog tcgplayer gap-fill") {
        RoutingHttpClient http;
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {"TamamushiCG":{"name_en":"Tamamushi City Gym"}},
            "prints": [
                {"set_id":"TamamushiCG","local_id":"016","name_en":"Erika",
                 "name_ja":"エリカ","name_en_source":"trainer-table",
                 "tcgplayer_id":"576776"}
            ]
        })");
        REQUIRE(catalog.isOk());
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        const auto out = src.fetchImageUrl("Erika", "TamamushiCG", "016");
        REQUIRE(out.isOk());
        CHECK(out.value() ==
              "https://product-images.tcgplayer.com/fit-in/437x437/576776.jpg");
    }

    TEST_CASE("neo catalog image_url gap-fills when TCGdex image is null") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonCardPreviewSource::buildCardUrl("neo4", "106")] =
            R"({"id":"neo4-106","name":"ラッキースタジアム","image":null})";
        http.bodies[JapanesePokemonCardPreviewSource::buildSetDetailUrl("neo4")] = R"({
            "id":"neo4",
            "cards":[{"localId":"106","name":"ラッキースタジアム"}]
        })";
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"neo4","local_id":"106","name_en":"Shining Celebi",
                 "name_ja":"輝くセレビ","name_en_source":"species-table-variant",
                 "image_url":"https://images.pokemontcg.io/neo4/106_hires.png"}
            ]
        })");
        REQUIRE(catalog.isOk());
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        const auto out = src.fetchImageUrl("Shining Celebi", "neo4", "106");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images.pokemontcg.io/neo4/106_hires.png");
    }
}

TEST_SUITE("JapanesePokemonCardPreviewSource::detectPrintVariants catalog-only") {
    TEST_CASE("English trainer name resolves when TCGdex set detail is unavailable") {
        RoutingHttpClient http;
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {"TamamushiCG":{"name_en":"Tamamushi City Gym"}},
            "prints": [
                {"set_id":"TamamushiCG","local_id":"021","name_en":"Celadon City Gym",
                 "name_ja":"タマムシシティジム","name_en_source":"trainer-table",
                 "image_url":"https://example.com/celadon.jpg"},
                {"set_id":"TamamushiCG","local_id":"001","name_en":"Erika's Oddish",
                 "name_ja":"エリカのナゾノクサ","name_en_source":"manual",
                 "image_url":"https://example.com/oddish.jpg"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        const auto out = src.detectPrintVariants("Celadon City Gym", "TamamushiCG");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "021");
    }

    TEST_CASE("detectPrintVariantsFromCatalog includes UnnumberedPromo prints without image_url") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {},
            "prints": [
                {"set_id":"UnnumberedPromo","local_id":"007","name_en":"Mewtwo (CoroCoro promo)",
                 "name_ja":"ミュウツー","name_en_source":"manual"},
                {"set_id":"UnnumberedPromo","local_id":"008","name_en":"Mewtwo (Fan Book promo)",
                 "name_ja":"ミュウツー","name_en_source":"manual"},
                {"set_id":"UnnumberedPromo","local_id":"030","name_en":"Mewtwo (WHF Special Sheet promo)",
                 "name_ja":"ミュウツー","name_en_source":"manual",
                 "image_url":"https://archives.bulbagarden.net/media/upload/w/w/MewtwoWHF.jpg"},
                {"set_id":"UnnumberedPromo","local_id":"045","name_en":"Mewtwo Strikes Back (Jumbo)",
                 "name_ja":"","name_en_source":"manual"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        const auto mew = JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
            "UnnumberedPromo", "Mewtwo", catalog.value());
        REQUIRE(mew.isOk());
        REQUIRE(mew.value().size() == 4);
        // Imaged prints first, then empty-URL identity rows.
        CHECK(mew.value()[0].setNo == "030");
        CHECK(mew.value()[1].setNo == "007");
        CHECK(mew.value()[2].setNo == "008");
        CHECK(mew.value()[3].setNo == "045");

        RoutingHttpClient http;
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        const auto img = src.fetchImageUrl("Mewtwo", "UnnumberedPromo", "030");
        REQUIRE(img.isOk());
        CHECK(img.value() ==
              "https://archives.bulbagarden.net/media/upload/w/w/MewtwoWHF.jpg");
    }

    TEST_CASE("fetchImageUrl does not borrow sibling UnnumberedPromo image") {
        RoutingHttpClient http;
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {},
            "prints": [
                {"set_id":"UnnumberedPromo","local_id":"007","name_en":"Mewtwo (CoroCoro promo)",
                 "name_ja":"ミュウツー","name_en_source":"manual"},
                {"set_id":"UnnumberedPromo","local_id":"030","name_en":"Mewtwo (WHF Special Sheet promo)",
                 "name_ja":"ミュウツー","name_en_source":"manual",
                 "image_url":"https://archives.bulbagarden.net/media/upload/w/w/MewtwoWHF.jpg"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        JapanesePokemonCardPreviewSource src{http, catalog.value()};
        const auto empty = src.fetchImageUrl("Mewtwo", "UnnumberedPromo", "007");
        REQUIRE(empty.isErr());
        CHECK(empty.error().kind == PreviewLookupError::Kind::NotFound);

        const auto whf = src.fetchImageUrl("Mewtwo", "UnnumberedPromo", "030");
        REQUIRE(whf.isOk());
        CHECK(whf.value() ==
              "https://archives.bulbagarden.net/media/upload/w/w/MewtwoWHF.jpg");
    }

    TEST_CASE("detectPrintVariantsFromCatalog dedupes shared preview URLs") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {},
            "prints": [
                {"set_id":"UnnumberedPromo","local_id":"030","name_en":"Mewtwo (WHF Special Sheet promo)",
                 "image_url":"https://archives.bulbagarden.net/media/upload/w/w/same.jpg"},
                {"set_id":"UnnumberedPromo","local_id":"073","name_en":"Mewtwo (Song Best Collection promo)",
                 "image_url":"https://archives.bulbagarden.net/media/upload/w/w/same.jpg"},
                {"set_id":"UnnumberedPromo","local_id":"197","name_en":"Mewtwo (Wizards Promo 12)",
                 "image_url":"https://archives.bulbagarden.net/media/upload/w/w/same.jpg"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        const auto mew = JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
            "UnnumberedPromo", "Mewtwo", catalog.value());
        REQUIRE(mew.isOk());
        REQUIRE(mew.value().size() == 1);
        CHECK(mew.value()[0].setNo == "030");
    }

    TEST_CASE("detectPrintVariantsFromCatalog matches owner Pokemon English title") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {},
            "prints": [
                {"set_id":"TamamushiCG","local_id":"001","name_en":"Erika's Oddish",
                 "name_ja":"エリカのナゾノクサ",
                 "image_url":"https://example.com/oddish.jpg"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        const auto out = JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
            "TamamushiCG", "Erika's Oddish", catalog.value());
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "001");
    }

    TEST_CASE("detectPrintVariantsFromCatalog matches Dark Rocket and Owner PMCG titles") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {},
            "prints": [
                {"set_id":"PMCG4","local_id":"017","name_en":"Dark Charizard",
                 "name_ja":"わるいリザードン","name_en_source":"species-table-variant",
                 "tcgplayer_id":"575744"},
                {"set_id":"PMCG6","local_id":"042","name_en":"Rocket's Zapdos",
                 "name_ja":"R団のサンダー","name_en_source":"species-table-variant",
                 "tcgplayer_id":"1"},
                {"set_id":"PMCG5","local_id":"002","name_en":"Erika's Oddish",
                 "name_ja":"エリカのナゾノクサ","name_en_source":"species-table-variant",
                 "tcgplayer_id":"2"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        const auto dark = JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
            "PMCG4", "Dark Charizard", catalog.value());
        REQUIRE(dark.isOk());
        REQUIRE(dark.value().size() == 1);
        CHECK(dark.value()[0].setNo == "017");

        const auto rocket = JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
            "PMCG6", "Rocket's Zapdos", catalog.value());
        REQUIRE(rocket.isOk());
        REQUIRE(rocket.value().size() == 1);
        CHECK(rocket.value()[0].setNo == "042");

        const auto owner = JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
            "PMCG5", "Erika's Oddish", catalog.value());
        REQUIRE(owner.isOk());
        REQUIRE(owner.value().size() == 1);
        CHECK(owner.value()[0].setNo == "002");
    }

    TEST_CASE("detectPrintVariantsFromCatalog matches Light and Shining neo titles") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {},
            "prints": [
                {"set_id":"neo4","local_id":"004","name_en":"Light Sunflora",
                 "name_ja":"軽いサンフロラ","name_en_source":"species-table-variant",
                 "image_url":"https://example.com/sunflora.png"},
                {"set_id":"neo4","local_id":"013","name_en":"Shining Celebi",
                 "name_ja":"輝くセレビ","name_en_source":"species-table-variant",
                 "image_url":"https://example.com/celebi.png"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        const auto light = JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
            "neo4", "Light Sunflora", catalog.value());
        REQUIRE(light.isOk());
        REQUIRE(light.value().size() == 1);
        CHECK(light.value()[0].setNo == "004");

        const auto shining = JapanesePokemonCardPreviewSource::detectPrintVariantsFromCatalog(
            "neo4", "Shining Celebi", catalog.value());
        REQUIRE(shining.isOk());
        REQUIRE(shining.value().size() == 1);
        CHECK(shining.value()[0].setNo == "013");
    }
}
