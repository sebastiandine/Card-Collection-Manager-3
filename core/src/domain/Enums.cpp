#include "ccm/domain/Enums.hpp"

#include <stdexcept>
#include <string>

#if defined(__GNUC__) || defined(__clang__)
#define CCM_UNREACHABLE() __builtin_unreachable()
#else
#define CCM_UNREACHABLE() ((void)0)
#endif

namespace ccm {

std::string_view to_string(Game g) noexcept {
    switch (g) {
        case Game::Magic:            return "Magic";
        case Game::Pokemon:          return "Pokemon";
        case Game::YuGiOh:           return "YuGiOh";
        case Game::DigiBattle99:     return "DigiBattle99";
        case Game::YuGiOhBandai:     return "YuGiOhBandai";
        case Game::JapanesePokemon:  return "JapanesePokemon";
    }
    CCM_UNREACHABLE();
}

std::string_view to_string(PokemonRegion r) noexcept {
    switch (r) {
        case PokemonRegion::West: return "West";
        case PokemonRegion::Asia: return "Asia";
    }
    CCM_UNREACHABLE();
}

std::string_view to_string(Language l) noexcept {
    switch (l) {
        case Language::English:            return "English";
        case Language::German:             return "German";
        case Language::French:             return "French";
        case Language::Spanish:            return "Spanish";
        case Language::Italian:            return "Italian";
        case Language::SimplifiedChinese:  return "S-Chinese";
        case Language::TraditionalChinese: return "T-Chinese";
        case Language::Japanese:           return "Japanese";
        case Language::Korean:             return "Korean";
        case Language::Russian:            return "Russian";
    }
    CCM_UNREACHABLE();
}

std::string_view to_string(Condition c) noexcept {
    switch (c) {
        case Condition::Mint:        return "Mint";
        case Condition::NearMint:    return "NearMint";
        case Condition::Excellent:   return "Excellent";
        case Condition::Good:        return "Good";
        case Condition::LightPlayed: return "LightPlayed";
        case Condition::Played:      return "Played";
        case Condition::Poor:        return "Poor";
    }
    CCM_UNREACHABLE();
}

std::string_view to_string(Theme t) noexcept {
    switch (t) {
        case Theme::Light: return "Light";
        case Theme::Dark:  return "Dark";
    }
    CCM_UNREACHABLE();
}

std::optional<Game> gameFromString(std::string_view s) noexcept {
    if (s == "Magic")           return Game::Magic;
    if (s == "Pokemon")         return Game::Pokemon;
    if (s == "YuGiOh")          return Game::YuGiOh;
    if (s == "DigiBattle99")    return Game::DigiBattle99;
    if (s == "YuGiOhBandai")    return Game::YuGiOhBandai;
    if (s == "JapanesePokemon") return Game::JapanesePokemon;
    return std::nullopt;
}

std::optional<PokemonRegion> pokemonRegionFromString(std::string_view s) noexcept {
    if (s == "West") return PokemonRegion::West;
    if (s == "Asia") return PokemonRegion::Asia;
    return std::nullopt;
}

std::optional<Language> languageFromString(std::string_view s) noexcept {
    if (s == "English")            return Language::English;
    if (s == "German")             return Language::German;
    if (s == "French")             return Language::French;
    if (s == "Spanish")            return Language::Spanish;
    if (s == "Italian")            return Language::Italian;
    if (s == "S-Chinese")          return Language::SimplifiedChinese;
    if (s == "T-Chinese")          return Language::TraditionalChinese;
    // Legacy single Chinese spelling → Simplified.
    if (s == "Chinese")            return Language::SimplifiedChinese;
    if (s == "Japanese")           return Language::Japanese;
    if (s == "Korean")             return Language::Korean;
    if (s == "Russian")            return Language::Russian;
    return std::nullopt;
}

std::optional<Condition> conditionFromString(std::string_view s) noexcept {
    if (s == "Mint")        return Condition::Mint;
    if (s == "NearMint")    return Condition::NearMint;
    if (s == "Excellent")   return Condition::Excellent;
    if (s == "Good")        return Condition::Good;
    if (s == "LightPlayed") return Condition::LightPlayed;
    if (s == "Played")      return Condition::Played;
    if (s == "Poor")        return Condition::Poor;
    return std::nullopt;
}

std::optional<Theme> themeFromString(std::string_view s) noexcept {
    if (s == "Light") return Theme::Light;
    if (s == "Dark")  return Theme::Dark;
    return std::nullopt;
}

const std::array<Game, 5>& allGames() noexcept {
    static constexpr std::array<Game, 5> v{
        Game::Magic, Game::Pokemon, Game::YuGiOh, Game::YuGiOhBandai,
        Game::DigiBattle99};
    return v;
}

const std::array<Language, 10>& allLanguages() noexcept {
    static constexpr std::array<Language, 10> v{
        Language::English, Language::German, Language::French, Language::Spanish,
        Language::Italian, Language::SimplifiedChinese, Language::TraditionalChinese,
        Language::Japanese, Language::Korean, Language::Russian
    };
    return v;
}

const std::array<Condition, 7>& allConditions() noexcept {
    static constexpr std::array<Condition, 7> v{
        Condition::Mint, Condition::NearMint, Condition::Excellent,
        Condition::Good, Condition::LightPlayed, Condition::Played, Condition::Poor
    };
    return v;
}

const std::array<Theme, 2>& allThemes() noexcept {
    static constexpr std::array<Theme, 2> v{Theme::Light, Theme::Dark};
    return v;
}

std::span<const Language> languagesForPokemonRegion(PokemonRegion r) noexcept {
    static constexpr std::array<Language, 6> kWest{
        Language::English, Language::German, Language::French,
        Language::Spanish, Language::Italian, Language::Russian};
    static constexpr std::array<Language, 4> kAsia{
        Language::Japanese, Language::SimplifiedChinese,
        Language::TraditionalChinese, Language::Korean};
    switch (r) {
        case PokemonRegion::West: return kWest;
        case PokemonRegion::Asia: return kAsia;
    }
    CCM_UNREACHABLE();
    return kWest;
}

Game pokemonBackendGame(PokemonRegion r) noexcept {
    return r == PokemonRegion::Asia ? Game::JapanesePokemon : Game::Pokemon;
}

Language defaultLanguageForPokemonRegion(PokemonRegion r) noexcept {
    return r == PokemonRegion::Asia ? Language::Japanese : Language::English;
}

void to_json(nlohmann::json& j, Game v)           { j = std::string(to_string(v)); }
void to_json(nlohmann::json& j, PokemonRegion v)  { j = std::string(to_string(v)); }
void to_json(nlohmann::json& j, Language v)       { j = std::string(to_string(v)); }
void to_json(nlohmann::json& j, Condition v)      { j = std::string(to_string(v)); }
void to_json(nlohmann::json& j, Theme v)          { j = std::string(to_string(v)); }

void from_json(const nlohmann::json& j, Game& v) {
    auto parsed = gameFromString(j.get<std::string>());
    if (!parsed) throw std::invalid_argument("Unknown Game value: " + j.get<std::string>());
    v = *parsed;
}
void from_json(const nlohmann::json& j, PokemonRegion& v) {
    auto parsed = pokemonRegionFromString(j.get<std::string>());
    if (!parsed) throw std::invalid_argument("Unknown PokemonRegion value: " + j.get<std::string>());
    v = *parsed;
}
void from_json(const nlohmann::json& j, Language& v) {
    auto parsed = languageFromString(j.get<std::string>());
    if (!parsed) throw std::invalid_argument("Unknown Language value: " + j.get<std::string>());
    v = *parsed;
}
void from_json(const nlohmann::json& j, Condition& v) {
    auto parsed = conditionFromString(j.get<std::string>());
    if (!parsed) throw std::invalid_argument("Unknown Condition value: " + j.get<std::string>());
    v = *parsed;
}

void from_json(const nlohmann::json& j, Theme& v) {
    auto parsed = themeFromString(j.get<std::string>());
    if (!parsed) throw std::invalid_argument("Unknown Theme value: " + j.get<std::string>());
    v = *parsed;
}

}  // namespace ccm
