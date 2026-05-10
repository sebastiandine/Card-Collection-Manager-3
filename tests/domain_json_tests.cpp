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
        c.rarityCode = "(UR)";
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
        CHECK(j.at("rarityCode") == "(UR)");
        CHECK(j.at("signed") == false);

        const YuGiOhCard back = j.get<YuGiOhCard>();
        CHECK(back == c);
    }

    TEST_CASE("missing rarityCode in legacy rows is accepted and mapped to empty") {
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
        CHECK(card.rarityCode.empty());
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

    TEST_CASE("Configuration missing required key throws") {
        const nlohmann::json j = {
            {"defaultGame", "Magic"},
            {"theme", "Dark"},
        };
        CHECK_THROWS(j.get<Configuration>());
    }
}
