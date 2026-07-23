#include <doctest/doctest.h>

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/games/pokemon/PokemonCollectionSetSync.hpp"

using namespace ccm;

namespace {

PokemonCard makeWest(std::string setId, std::string setName, std::string releaseDate) {
    PokemonCard c;
    c.id = 1;
    c.name = "Card";
    c.region = PokemonRegion::West;
    c.set.id = std::move(setId);
    c.set.name = std::move(setName);
    c.set.releaseDate = std::move(releaseDate);
    c.setNo = "1";
    c.language = Language::English;
    return c;
}

PokemonCard makeAsia(std::string setId, std::string setName, std::string releaseDate) {
    PokemonCard c = makeWest(std::move(setId), std::move(setName), std::move(releaseDate));
    c.region = PokemonRegion::Asia;
    c.language = Language::Japanese;
    return c;
}

}  // namespace

TEST_SUITE("syncPokemonCollectionSets") {
    TEST_CASE("migrates West legacy set id and refreshes name/date") {
        std::vector<PokemonCard> cards{
            makeWest("sv1", "Old Name", "2000/01/01"),
        };
        const std::vector<Set> west{Set{"sv01", "Scarlet & Violet", "2023/03/31"}};
        const std::vector<Set> asia;

        CHECK(syncPokemonCollectionSets(cards, west, asia) == 1);
        CHECK(cards[0].set.id == "sv01");
        CHECK(cards[0].set.name == "Scarlet & Violet");
        CHECK(cards[0].set.releaseDate == "2023/03/31");
    }

    TEST_CASE("Asia cards refresh metadata without West id aliases") {
        std::vector<PokemonCard> cards{
            makeAsia("sv1", "Old", "2000/01/01"),
        };
        const std::vector<Set> west{Set{"sv01", "Scarlet & Violet", "2023/03/31"}};
        const std::vector<Set> asia{Set{"sv1", "Asia Set", "2023/01/20"}};

        CHECK(syncPokemonCollectionSets(cards, west, asia) == 1);
        CHECK(cards[0].set.id == "sv1");
        CHECK(cards[0].set.name == "Asia Set");
        CHECK(cards[0].set.releaseDate == "2023/01/20");
    }

    TEST_CASE("unchanged cards are not counted") {
        std::vector<PokemonCard> cards{
            makeWest("base1", "Base Set", "1999/01/09"),
        };
        const std::vector<Set> west{Set{"base1", "Base Set", "1999/01/09"}};
        CHECK(syncPokemonCollectionSets(cards, west, {}) == 0);
    }

    TEST_CASE("unknown set id still migrates when aliased") {
        std::vector<PokemonCard> cards{makeWest("pgo", "GO", "")};
        // No matching upstream set — id still migrates.
        CHECK(syncPokemonCollectionSets(cards, {}, {}) == 1);
        CHECK(cards[0].set.id == "swsh10.5");
        CHECK(cards[0].set.name == "GO");
    }
}
