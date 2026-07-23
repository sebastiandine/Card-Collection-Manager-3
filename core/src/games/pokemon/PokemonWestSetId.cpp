#include "ccm/games/pokemon/PokemonWestSetId.hpp"

#include <unordered_map>

namespace ccm {

namespace {

// Built by name-matching PokemonTCG/pokemon-tcg-data set ids against
// api.tcgdex.net/v2/en/sets. Only divergences are listed; shared ids
// (base1, swsh1, sv10, sve, …) pass through unchanged.
const std::unordered_map<std::string, std::string>& legacyAliases() {
    static const std::unordered_map<std::string, std::string> kMap{
        // Classic / EX / HGSS renames
        {"base6", "lc"},
        {"bp", "bog"},
        {"tk1a", "tk-ex-latia"},
        {"tk1b", "tk-ex-latio"},
        {"tk2a", "tk-ex-p"},
        {"tk2b", "tk-ex-m"},
        {"hsp", "hgssp"},

        // McDonald's Collections
        {"mcd11", "2011bw"},
        {"mcd12", "2012bw"},
        {"mcd14", "2014xy"},
        {"mcd15", "2015xy"},
        {"mcd16", "2016xy"},
        {"mcd17", "2017sm"},
        {"mcd18", "2018sm"},
        {"mcd19", "2019sm"},
        {"mcd21", "2021swsh"},
        {"mcd22", "2022swsh"},
        {"mcd23", "2023sv"},
        {"mcd24", "2024sv"},

        // SM specials
        {"sm35", "sm3.5"},
        {"sm75", "sm7.5"},

        // SWSH specials / galleries
        {"swsh35", "swsh3.5"},
        {"swsh45", "swsh4.5"},
        {"swsh45sv", "swsh4.5sv"},
        {"cel25c", "cel25cc"},
        {"swsh9tg", "swsh9.5tg"},
        {"swsh10tg", "swsh10.5tg"},
        {"pgo", "swsh10.5"},
        {"swsh11tg", "swsh11.5tg"},
        {"swsh12tg", "swsh12.5tg"},
        {"swsh12pt5", "swsh12.5"},
        {"swsh12pt5gg", "swsh12.5gg"},

        // Scarlet & Violet (pokemontcg used unpadded / pt5 forms)
        {"sv1", "sv01"},
        {"sv2", "sv02"},
        {"sv3", "sv03"},
        {"sv3pt5", "sv03.5"},
        {"sv4", "sv04"},
        {"sv4pt5", "sv04.5"},
        {"sv5", "sv05"},
        {"sv6", "sv06"},
        {"sv6pt5", "sv06.5"},
        {"sv7", "sv07"},
        {"sv8", "sv08"},
        {"sv8pt5", "sv08.5"},
        {"sv9", "sv09"},
        {"zsv10pt5", "sv10.5b"},
        {"rsv10pt5", "sv10.5w"},

        // Mega Evolution era
        {"me1", "me01"},
        {"me2", "me02"},
        {"me2pt5", "me02.5"},
        {"me3", "me03"},
        {"me4", "me04"},
        {"me5", "me05"},
    };
    return kMap;
}

}  // namespace

std::string canonicalizeWestSetId(std::string_view setId) {
    if (setId.empty()) return {};
    const auto& map = legacyAliases();
    const auto it = map.find(std::string(setId));
    if (it != map.end()) return it->second;
    return std::string(setId);
}

}  // namespace ccm
