#include <doctest/doctest.h>

#include "ccm/domain/Configuration.hpp"
#include "ccm/domain/Enums.hpp"
#include "ccm/domain/MagicCard.hpp"
#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/domain/Set.hpp"

#include <nlohmann/json.hpp>

using namespace ccm;

TEST_SUITE("domain enums round-trip JSON as strings") {
    TEST_CASE("Game") {
        nlohmann::json j = Game::Magic;
        CHECK(j.get<std::string>() == "Magic");
        CHECK(j.get<Game>() == Game::Magic);

        nlohmann::json j2 = "Pokemon";
        CHECK(j2.get<Game>() == Game::Pokemon);

        nlohmann::json jYgo = "YuGiOh";
        CHECK(jYgo.get<Game>() == Game::YuGiOh);

        nlohmann::json j3 = Theme::Dark;
        CHECK(j3.get<std::string>() == "Dark");
        CHECK(j3.get<Theme>() == Theme::Dark);
    }

    TEST_CASE("Language and Condition") {
        nlohmann::json l = Language::Japanese;
        CHECK(l.get<std::string>() == "Japanese");
        CHECK(l.get<Language>() == Language::Japanese);

        nlohmann::json c = Condition::LightPlayed;
        CHECK(c.get<std::string>() == "LightPlayed");
        CHECK(c.get<Condition>() == Condition::LightPlayed);
    }

    TEST_CASE("invalid enum string throws") {
        nlohmann::json bad = "Spanglish";
        CHECK_THROWS(bad.get<Language>());
    }

    TEST_CASE("all enum values round-trip through string helpers") {
        for (const auto game : allGames()) {
            const auto name = to_string(game);
            CHECK(gameFromString(name).has_value());
            CHECK(*gameFromString(name) == game);
        }

        for (const auto language : allLanguages()) {
            const auto name = to_string(language);
            CHECK(languageFromString(name).has_value());
            CHECK(*languageFromString(name) == language);
        }

        for (const auto condition : allConditions()) {
            const auto name = to_string(condition);
            CHECK(conditionFromString(name).has_value());
            CHECK(*conditionFromString(name) == condition);
        }

        for (const auto theme : allThemes()) {
            const auto name = to_string(theme);
            CHECK(themeFromString(name).has_value());
            CHECK(*themeFromString(name) == theme);
        }
    }

    TEST_CASE("invalid enum helper inputs return nullopt") {
        CHECK_FALSE(gameFromString("magic").has_value());
        CHECK_FALSE(languageFromString("EN").has_value());
        CHECK_FALSE(conditionFromString("Near Mint").has_value());
        CHECK_FALSE(themeFromString("OLED").has_value());
    }

    TEST_CASE("invalid game, condition and theme JSON values throw") {
        nlohmann::json badGame = "YGO";
        CHECK_THROWS(badGame.get<Game>());

        nlohmann::json badCondition = "Pristine";
        CHECK_THROWS(badCondition.get<Condition>());

        nlohmann::json badTheme = "Midnight";
        CHECK_THROWS(badTheme.get<Theme>());
    }
}

TEST_SUITE("Set JSON shape stays stable") {
    TEST_CASE("uses 'releaseDate' alias") {
        Set s{"abc", "Test Set", "2024/05/01"};
        const nlohmann::json j = s;
        CHECK(j.contains("releaseDate"));
        CHECK_FALSE(j.contains("release_date"));
        CHECK(j.at("releaseDate") == "2024/05/01");

        const Set back = j.get<Set>();
        CHECK(back == s);
    }
}

TEST_SUITE("MagicCard JSON") {
    TEST_CASE("round-trips with all original field names") {
        MagicCard c;
        c.id = 42;
        c.amount = 3;
        c.name = "Lightning Bolt";
        c.set = Set{"lea", "Limited Edition Alpha", "1993/08/05"};
        c.note = "rare promo";
        c.images = {"foo+bar+0.png"};
        c.language = Language::English;
        c.condition = Condition::NearMint;
        c.foil = true;
        c.signed_ = false;
        c.altered = false;

        nlohmann::json j = c;
        CHECK(j.contains("signed"));
        CHECK(j.at("signed") == false);
        CHECK(j.at("foil") == true);

        const MagicCard back = j.get<MagicCard>();
        CHECK(back == c);
    }
}

TEST_SUITE("PokemonCard JSON") {
    TEST_CASE("uses 'setNo' and 'firstEdition' aliases") {
        PokemonCard c;
        c.id = 7;
        c.amount = 1;
        c.name = "Charizard";
        c.set = Set{"base1", "Base Set", "1999/01/09"};
        c.setNo = "4/102";
        c.note = "";
        c.images = {};
        c.language = Language::English;
        c.condition = Condition::Excellent;
        c.firstEdition = true;
        c.holo = true;
        c.signed_ = false;
        c.altered = false;

        nlohmann::json j = c;
        CHECK(j.at("setNo") == "4/102");
        CHECK(j.at("firstEdition") == true);
        CHECK(j.at("signed") == false);

        const PokemonCard back = j.get<PokemonCard>();
        CHECK(back == c);
    }
}

TEST_SUITE("Configuration JSON matches Rust serde aliases") {
    TEST_CASE("dataStorage / defaultGame / theme keys are present") {
        Configuration cfg;
        cfg.dataStorage = "/some/path";
        cfg.defaultGame = Game::Pokemon;
        cfg.theme = Theme::Dark;

        nlohmann::json j = cfg;
        CHECK(j.at("dataStorage") == "/some/path");
        CHECK(j.at("defaultGame") == "Pokemon");
        CHECK(j.at("theme") == "Dark");

        const auto back = j.get<Configuration>();
        CHECK(back == cfg);
    }

    TEST_CASE("missing theme key defaults to Light") {
        const nlohmann::json j = {
            {"dataStorage", "/portable/data"},
            {"defaultGame", "YuGiOh"},
        };

        const auto cfg = j.get<Configuration>();
        CHECK(cfg.dataStorage == "/portable/data");
        CHECK(cfg.defaultGame == Game::YuGiOh);
        CHECK(cfg.theme == Theme::Light);
    }
}

TEST_SUITE("YuGiOhCard JSON") {
    TEST_CASE("round-trips with setNo and rarity fields") {
        YuGiOhCard c;
        c.id = 77;
        c.amount = 2;
        c.name = "Blue-Eyes White Dragon";
        c.set = Set{"SDK-001", "Starter Deck Kaiba", "2002/03/29"};
        c.setNo = "SDK-001";
        c.rarity = "Ultra Rare";
        c.note = "classic";
        c.images = {"77+starter+blue-eyes+0.png"};
        c.language = Language::English;
        c.condition = Condition::NearMint;
        c.firstEdition = true;
        c.signed_ = false;
        c.altered = false;

        nlohmann::json j = c;
        CHECK(j.at("setNo") == "SDK-001");
        CHECK(j.at("rarity") == "Ultra Rare");
        CHECK_FALSE(j.contains("rarityCode"));
        CHECK(j.at("signed") == false);

        const YuGiOhCard back = j.get<YuGiOhCard>();
        CHECK(back == c);
    }

    TEST_CASE("legacy JSON without rarityCode key parses (domain uses rarity only)") {
        const nlohmann::json j = {
            {"id", 1},
            {"amount", 1},
            {"name", "Dark Magician"},
            {"set", nlohmann::json{
                {"id", "SDY"},
                {"name", "Starter Deck: Yugi"},
                {"releaseDate", "2002/03/29"},
            }},
            {"setNo", "SDY-006"},
            {"rarity", "Ultra Rare"},
            {"note", ""},
            {"images", nlohmann::json::array()},
            {"language", "English"},
            {"condition", "NearMint"},
            {"firstEdition", false},
            {"signed", false},
            {"altered", false},
        };

        const auto card = j.get<YuGiOhCard>();
        CHECK(card.rarity == "Ultra Rare");
        CHECK(card.setNo == "SDY-006");
        CHECK(card.set.id == "SDY");
    }

    TEST_CASE("serializes non-default flags and metadata fields") {
        YuGiOhCard c;
        c.id = 3;
        c.amount = 4;
        c.name = "Red-Eyes Black Dragon";
        c.set = Set{"lob", "Legend of Blue Eyes", "2002/03/08"};
        c.setNo = "LOB-070";
        c.rarity = "Secret Rare";
        c.note = "graded";
        c.images = {};
        c.language = Language::German;
        c.condition = Condition::Played;
        c.firstEdition = false;
        c.signed_ = true;
        c.altered = true;

        const nlohmann::json j = c;
        CHECK(j.at("firstEdition") == false);
        CHECK(j.at("signed") == true);
        CHECK(j.at("altered") == true);
        CHECK(j.at("language") == "German");
        CHECK(j.at("condition") == "Played");
        CHECK(j.at("images") == nlohmann::json::array());

        const YuGiOhCard back = j.get<YuGiOhCard>();
        CHECK(back == c);
    }

    TEST_CASE("operator== distinguishes each field") {
        YuGiOhCard base;
        base.id = 10;
        base.amount = 2;
        base.name = "Dark Magician";
        base.set = Set{"lob", "Legend of Blue Eyes", "2002/03/08"};
        base.setNo = "LOB-005";
        base.rarity = "Ultra Rare";
        base.note = "note";
        base.images = {"a.png"};
        base.language = Language::English;
        base.condition = Condition::NearMint;
        base.firstEdition = true;
        base.signed_ = false;
        base.altered = false;

        auto changed = base;
        changed.id = 11;
        CHECK_FALSE(changed == base);

        changed = base;
        changed.amount = 3;
        CHECK_FALSE(changed == base);

        changed = base;
        changed.name = "Other";
        CHECK_FALSE(changed == base);

        changed = base;
        changed.set.name = "Other Set";
        CHECK_FALSE(changed == base);

        changed = base;
        changed.setNo = "LOB-006";
        CHECK_FALSE(changed == base);

        changed = base;
        changed.rarity = "Rare";
        CHECK_FALSE(changed == base);

        changed = base;
        changed.note = "other";
        CHECK_FALSE(changed == base);

        changed = base;
        changed.images = {};
        CHECK_FALSE(changed == base);

        changed = base;
        changed.language = Language::Japanese;
        CHECK_FALSE(changed == base);

        changed = base;
        changed.condition = Condition::Played;
        CHECK_FALSE(changed == base);

        changed = base;
        changed.firstEdition = false;
        CHECK_FALSE(changed == base);

        changed = base;
        changed.signed_ = true;
        CHECK_FALSE(changed == base);

        changed = base;
        changed.altered = true;
        CHECK_FALSE(changed == base);
    }
}

TEST_SUITE("Domain JSON required fields") {
    TEST_CASE("Set missing required key throws") {
        const nlohmann::json j = {
            {"id", "lea"},
            {"name", "Limited Edition Alpha"},
        };
        CHECK_THROWS(j.get<Set>());
    }

    TEST_CASE("MagicCard missing required key throws") {
        const nlohmann::json j = {
            {"id", 10},
            {"amount", 1},
            {"name", "Lightning Bolt"},
            {"set", nlohmann::json{
                {"id", "lea"},
                {"name", "Limited Edition Alpha"},
                {"releaseDate", "1993/08/05"},
            }},
            // note missing on purpose
            {"images", nlohmann::json::array()},
            {"language", "English"},
            {"condition", "NearMint"},
            {"foil", false},
            {"signed", false},
            {"altered", false},
        };
        CHECK_THROWS(j.get<MagicCard>());
    }

    TEST_CASE("PokemonCard missing required key throws") {
        const nlohmann::json j = {
            {"id", 7},
            {"amount", 1},
            {"name", "Charizard"},
            {"set", nlohmann::json{
                {"id", "base1"},
                {"name", "Base Set"},
                {"releaseDate", "1999/01/09"},
            }},
            {"setNo", "4/102"},
            {"note", ""},
            {"images", nlohmann::json::array()},
            {"language", "English"},
            {"condition", "Excellent"},
            {"firstEdition", true},
            // holo missing on purpose
            {"signed", false},
            {"altered", false},
        };
        CHECK_THROWS(j.get<PokemonCard>());
    }

    TEST_CASE("YuGiOhCard missing required key throws") {
        const nlohmann::json j = {
            {"id", 7},
            {"amount", 1},
            {"name", "Blue-Eyes White Dragon"},
            {"set", nlohmann::json{
                {"id", "sdk"},
                {"name", "Starter Deck Kaiba"},
                {"releaseDate", "2002/03/29"},
            }},
            {"setNo", "SDK-001"},
            {"note", ""},
            {"images", nlohmann::json::array()},
            {"language", "English"},
            {"condition", "NearMint"},
            {"firstEdition", true},
            // rarity missing on purpose
            {"signed", false},
            {"altered", false},
        };
        CHECK_THROWS(j.get<YuGiOhCard>());
    }

    TEST_CASE("YuGiOhCard missing each required key throws") {
        const nlohmann::json full = {
            {"id", 7},
            {"amount", 1},
            {"name", "Blue-Eyes White Dragon"},
            {"set", nlohmann::json{
                {"id", "sdk"},
                {"name", "Starter Deck Kaiba"},
                {"releaseDate", "2002/03/29"},
            }},
            {"setNo", "SDK-001"},
            {"rarity", "Ultra Rare"},
            {"note", ""},
            {"images", nlohmann::json::array()},
            {"language", "English"},
            {"condition", "NearMint"},
            {"firstEdition", true},
            {"signed", false},
            {"altered", false},
        };

        for (const char* key : {
                 "id", "amount", "name", "set", "setNo", "note", "images",
                 "language", "condition", "firstEdition", "rarity", "signed", "altered"}) {
            nlohmann::json partial = full;
            partial.erase(key);
            CHECK_THROWS(partial.get<YuGiOhCard>());
        }
    }

    TEST_CASE("Configuration missing required key throws") {
        const nlohmann::json j = {
            {"defaultGame", "Magic"},
            {"theme", "Dark"},
        };
        CHECK_THROWS(j.get<Configuration>());
    }

    TEST_CASE("Configuration invalid theme value throws when present") {
        const nlohmann::json j = {
            {"dataStorage", "/portable/data"},
            {"defaultGame", "Magic"},
            {"theme", "Neon"},
        };
        CHECK_THROWS(j.get<Configuration>());
    }
}
