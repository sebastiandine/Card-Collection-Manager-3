#include <doctest/doctest.h>

#include "ccm/domain/MagicCard.hpp"
#include "ccm/services/MagicDeckCheck.hpp"

using namespace ccm;

namespace {

MagicCard makeCard(std::uint32_t id, std::string name, std::uint8_t amount,
                   std::string setName = "Alpha",
                   Language language = Language::English,
                   Condition condition = Condition::NearMint) {
    MagicCard c;
    c.id = id;
    c.name = std::move(name);
    c.amount = amount;
    c.set.id = "lea";
    c.set.name = std::move(setName);
    c.language = language;
    c.condition = condition;
    return c;
}

}  // namespace

TEST_SUITE("parseMagicDeckList") {
    TEST_CASE("parses N and Nx prefixes") {
        const auto lines = parseMagicDeckList("4 Lightning Bolt\n2x Counterspell\n");
        REQUIRE(lines.size() == 2);
        CHECK(lines[0].name == "Lightning Bolt");
        CHECK(lines[0].required == 4);
        CHECK(lines[1].name == "Counterspell");
        CHECK(lines[1].required == 2);
    }

    TEST_CASE("strips Arena set and collector suffix") {
        const auto lines =
            parseMagicDeckList("4 Lightning Bolt (LEA) 161\n1 Sol Ring (2X2) 311\n");
        REQUIRE(lines.size() == 2);
        CHECK(lines[0].name == "Lightning Bolt");
        CHECK(lines[0].required == 4);
        CHECK(lines[1].name == "Sol Ring");
        CHECK(lines[1].required == 1);
    }

    TEST_CASE("skips blanks comments and section headers") {
        const auto lines = parseMagicDeckList(
            "Deck\n"
            "4 Lightning Bolt\n"
            "\n"
            "// comment\n"
            "# also a comment\n"
            "Sideboard:\n"
            "2 Negate\n"
            "Commander\n"
            "Companion\n"
            "Maybeboard\n"
            "junk without a number\n");
        REQUIRE(lines.size() == 2);
        CHECK(lines[0].name == "Lightning Bolt");
        CHECK(lines[1].name == "Negate");
        CHECK(lines[1].required == 2);
    }

    TEST_CASE("merges duplicate names and collapses whitespace") {
        const auto lines = parseMagicDeckList(
            "2 Lightning  Bolt\n"
            "2x lightning bolt (M10) 146\n");
        REQUIRE(lines.size() == 1);
        CHECK(lines[0].name == "Lightning Bolt");
        CHECK(lines[0].required == 4);
    }

    TEST_CASE("skips zero quantity and name-only lines") {
        const auto lines = parseMagicDeckList("0 Lightning Bolt\nLightning Bolt\n");
        CHECK(lines.empty());
    }

    TEST_CASE("includes SB prefixed sideboard lines") {
        const auto lines = parseMagicDeckList("4 Lightning Bolt\nSB: 2 Negate\n");
        REQUIRE(lines.size() == 2);
        CHECK(lines[1].name == "Negate");
        CHECK(lines[1].required == 2);
    }
}

TEST_SUITE("analyzeMagicDeckCheck") {
    TEST_CASE("zero owned is missing only") {
        const auto deck = parseMagicDeckList("4 Lightning Bolt\n");
        const auto result = analyzeMagicDeckCheck(deck, {});
        REQUIRE(result.missing.size() == 1);
        CHECK(result.owned.empty());
        CHECK(result.missing[0].name == "Lightning Bolt");
        CHECK(result.missing[0].required == 4);
        CHECK(result.missing[0].missingAmount == 4);
    }

    TEST_CASE("exact owned amount is owned only") {
        const auto deck = parseMagicDeckList("4 Lightning Bolt\n");
        std::vector<MagicCard> collection{makeCard(1, "Lightning Bolt", 4)};
        const auto result = analyzeMagicDeckCheck(deck, collection);
        CHECK(result.missing.empty());
        REQUIRE(result.owned.size() == 1);
        CHECK(result.owned[0].ownedAmount == 4);
        CHECK(result.owned[0].required == 4);
        CHECK(result.owned[0].variations.size() == 1);
    }

    TEST_CASE("partial copies appear on both lists") {
        const auto deck = parseMagicDeckList("4 Lightning Bolt\n");
        std::vector<MagicCard> collection{makeCard(1, "Lightning Bolt", 2)};
        const auto result = analyzeMagicDeckCheck(deck, collection);
        REQUIRE(result.missing.size() == 1);
        REQUIRE(result.owned.size() == 1);
        CHECK(result.missing[0].missingAmount == 2);
        CHECK(result.missing[0].required == 4);
        CHECK(result.owned[0].ownedAmount == 2);
        CHECK(result.owned[0].required == 4);
    }

    TEST_CASE("surplus copies are owned only") {
        const auto deck = parseMagicDeckList("4 Lightning Bolt\n");
        std::vector<MagicCard> collection{makeCard(1, "Lightning Bolt", 7)};
        const auto result = analyzeMagicDeckCheck(deck, collection);
        CHECK(result.missing.empty());
        REQUIRE(result.owned.size() == 1);
        CHECK(result.owned[0].ownedAmount == 7);
    }

    TEST_CASE("sums all printings of the same name") {
        const auto deck = parseMagicDeckList("4 Lightning Bolt\n");
        std::vector<MagicCard> collection{
            makeCard(1, "Lightning Bolt", 1, "Alpha"),
            makeCard(2, "lightning  bolt", 1, "Magic 2011", Language::German,
                     Condition::Excellent),
        };
        const auto result = analyzeMagicDeckCheck(deck, collection);
        REQUIRE(result.missing.size() == 1);
        REQUIRE(result.owned.size() == 1);
        CHECK(result.missing[0].missingAmount == 2);
        CHECK(result.owned[0].ownedAmount == 2);
        REQUIRE(result.owned[0].variations.size() == 2);
        CHECK(result.owned[0].variations[0].set.name == "Alpha");
        CHECK(result.owned[0].variations[1].set.name == "Magic 2011");
        CHECK(result.owned[0].variations[1].language == Language::German);
        CHECK(result.owned[0].variations[1].condition == Condition::Excellent);
    }

    TEST_CASE("two different names stay independent") {
        const auto deck = parseMagicDeckList("4 Lightning Bolt\n1 Sol Ring\n");
        std::vector<MagicCard> collection{makeCard(1, "Sol Ring", 1)};
        const auto result = analyzeMagicDeckCheck(deck, collection);
        REQUIRE(result.missing.size() == 1);
        CHECK(result.missing[0].name == "Lightning Bolt");
        REQUIRE(result.owned.size() == 1);
        CHECK(result.owned[0].name == "Sol Ring");
    }
}
