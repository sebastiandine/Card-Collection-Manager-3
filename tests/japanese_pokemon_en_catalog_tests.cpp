#include <doctest/doctest.h>

#include "ccm/games/pokemonjp/JapanesePokemonEnCatalog.hpp"

using namespace ccm;

TEST_SUITE("JapanesePokemonEnCatalog") {
    TEST_CASE("parses sets and prints") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {
                "PMCG1": {
                    "name_en": "Expansion Pack",
                    "name_ja": "拡張パック",
                    "releaseDate": "1996/10/20"
                }
            },
            "prints": [
                {
                    "set_id": "PMCG1",
                    "local_id": "001",
                    "name_en": "Charmander",
                    "name_ja": "ヒトカゲ",
                    "name_en_source": "bulbapedia"
                }
            ]
        })");
        REQUIRE(catalog.isOk());
        CHECK_FALSE(catalog.value().empty());

        auto set = catalog.value().findSet("PMCG1");
        REQUIRE(set.has_value());
        CHECK(set->nameEn == "Expansion Pack");
        CHECK(set->releaseDate == "1996/10/20");

        auto print = catalog.value().findPrint("PMCG1", "001");
        REQUIRE(print.has_value());
        CHECK(print->nameEn == "Charmander");
        CHECK(print->nameEnSource == "bulbapedia");
    }

    TEST_CASE("parses optional tcgplayer_id and image_url") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {
                    "set_id": "PMCG1",
                    "local_id": "021",
                    "name_en": "Charizard",
                    "name_ja": "リザードン",
                    "tcgplayer_id": 575604
                },
                {
                    "set_id": "SV1a",
                    "local_id": "001",
                    "name_en": "Tropius",
                    "image_url": "https://example.com/tropius.png"
                }
            ]
        })");
        REQUIRE(catalog.isOk());
        auto charizard = catalog.value().findPrint("PMCG1", "021");
        REQUIRE(charizard.has_value());
        CHECK(charizard->tcgplayerId == "575604");
        CHECK(JapanesePokemonEnCatalog::previewImageUrlFromPrint(*charizard) ==
              "https://product-images.tcgplayer.com/fit-in/437x437/575604.jpg");

        auto tropius = catalog.value().findPrint("SV1a", "001");
        REQUIRE(tropius.has_value());
        CHECK(JapanesePokemonEnCatalog::previewImageUrlFromPrint(*tropius) ==
              "https://example.com/tropius.png");
    }

    TEST_CASE("findPrintsByName is case-insensitive on English") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"PMCG1","local_id":"001","name_en":"Charmander","name_ja":"ヒトカゲ"}
            ]
        })");
        REQUIRE(catalog.isOk());
        const auto hits = catalog.value().findPrintsByName("PMCG1", "charmander");
        REQUIRE(hits.size() == 1);
        CHECK(hits[0].localId == "001");
    }

    TEST_CASE("findPrintsByName matches qualified English titles by bare prefix") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {},
            "prints": [
                {"set_id":"UnnumberedPromo","local_id":"007","name_en":"Mewtwo (CoroCoro promo)"},
                {"set_id":"UnnumberedPromo","local_id":"008","name_en":"Mewtwo (Fan Book promo)"},
                {"set_id":"UnnumberedPromo","local_id":"045","name_en":"Mewtwo Strikes Back (Jumbo)"},
                {"set_id":"UnnumberedPromo","local_id":"001","name_en":"Pikachu (CoroCoro promo)"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        const auto hits = catalog.value().findPrintsByName("UnnumberedPromo", "Mewtwo");
        REQUIRE(hits.size() == 3);
        CHECK(hits[0].localId == "007");
        CHECK(hits[1].localId == "008");
        CHECK(hits[2].localId == "045");
        // Exact full title still works.
        const auto exact = catalog.value().findPrintsByName(
            "UnnumberedPromo", "Mewtwo Strikes Back (Jumbo)");
        REQUIRE(exact.size() == 1);
        CHECK(exact[0].localId == "045");
    }

    TEST_CASE("findPrintsByName whole-token matches owner and GR titles") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"json({
            "sets": {},
            "prints": [
                {"set_id":"UnnumberedPromo","local_id":"227","name_en":"Team GR's Mewtwo (Pokémon Card GB2 promo)"},
                {"set_id":"UnnumberedPromo","local_id":"045","name_en":"Mewtwo Strikes Back (CoroCoro promo) (Jumbo)"},
                {"set_id":"UnnumberedPromo","local_id":"016","name_en":"Mew (CoroCoro promo)"}
            ]
        })json");
        REQUIRE(catalog.isOk());
        const auto mewtwo = catalog.value().findPrintsByName("UnnumberedPromo", "Mewtwo");
        REQUIRE(mewtwo.size() == 2);
        CHECK(mewtwo[0].localId == "227");
        CHECK(mewtwo[1].localId == "045");

        // "Mew" must not match "Mewtwo …" rows.
        const auto mew = catalog.value().findPrintsByName("UnnumberedPromo", "Mew");
        REQUIRE(mew.size() == 1);
        CHECK(mew[0].localId == "016");
    }

    TEST_CASE("hasPrintsForSet reports curated classic products") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"TamamushiCG","local_id":"021","name_en":"Celadon City Gym"}
            ]
        })");
        REQUIRE(catalog.isOk());
        CHECK(catalog.value().hasPrintsForSet("TamamushiCG"));
        CHECK_FALSE(catalog.value().hasPrintsForSet("PMCG1"));
    }

    TEST_CASE("printsForSet returns all prints for a set id") {
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {},
            "prints": [
                {"set_id":"A","local_id":"1","name_en":"One"},
                {"set_id":"A","local_id":"2","name_en":"Two"},
                {"set_id":"B","local_id":"1","name_en":"Other"}
            ]
        })");
        REQUIRE(catalog.isOk());
        const auto prints = catalog.value().printsForSet("A");
        REQUIRE(prints.size() == 2);
        CHECK(catalog.value().printsForSet("missing").empty());
    }

    TEST_CASE("missing set/print returns nullopt") {
        JapanesePokemonEnCatalog empty;
        CHECK_FALSE(empty.findSet("X").has_value());
        CHECK_FALSE(empty.findPrint("X", "1").has_value());
        CHECK(empty.empty());
    }

    TEST_CASE("malformed JSON is an error") {
        CHECK(JapanesePokemonEnCatalog::parse("{not json").isErr());
    }
}
