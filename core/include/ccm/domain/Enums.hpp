#pragma once

// Game / Language / Condition enums.
//
// String spellings are kept identical to the established Rust serde defaults
// (e.g. `NearMint`, `LightPlayed`, `Magic`, `Pokemon`) so existing JSON files
// remain interchangeable.

#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ccm {

enum class Game {
    Magic,
    Pokemon,
    YuGiOh,
    DigiBattle99,
    JapanesePokemon,  // internal Asia sets/preview routing; not in allGames()
};

enum class PokemonRegion {
    West,
    Asia,
};

enum class Language {
    English,
    German,
    French,
    Spanish,
    Italian,
    SimplifiedChinese,   // JSON / display: "S-Chinese" (legacy "Chinese" accepted)
    TraditionalChinese,  // JSON / display: "T-Chinese"
    Japanese,
    Korean,
    Russian,
};

enum class Condition {
    Mint,
    NearMint,
    Excellent,
    Good,
    LightPlayed,
    Played,
    Poor,
};

enum class Theme {
    Light,
    Dark,
};

std::string_view to_string(Game g) noexcept;
std::string_view to_string(PokemonRegion r) noexcept;
std::string_view to_string(Language l) noexcept;
std::string_view to_string(Condition c) noexcept;
std::string_view to_string(Theme t) noexcept;

std::optional<Game>          gameFromString(std::string_view s) noexcept;
std::optional<PokemonRegion> pokemonRegionFromString(std::string_view s) noexcept;
std::optional<Language>      languageFromString(std::string_view s) noexcept;
std::optional<Condition>     conditionFromString(std::string_view s) noexcept;
std::optional<Theme>         themeFromString(std::string_view s) noexcept;

// User-facing games (Game menu / Settings). JapanesePokemon is internal-only.
const std::array<Game, 4>&       allGames() noexcept;
const std::array<Language, 10>&  allLanguages() noexcept;
const std::array<Condition, 7>&  allConditions() noexcept;
const std::array<Theme, 2>&      allThemes() noexcept;

[[nodiscard]] std::span<const Language> languagesForPokemonRegion(PokemonRegion r) noexcept;
[[nodiscard]] Game pokemonBackendGame(PokemonRegion r) noexcept;
[[nodiscard]] Language defaultLanguageForPokemonRegion(PokemonRegion r) noexcept;

// nlohmann/json hooks - serialize as plain strings, matching Rust serde.
void to_json(nlohmann::json& j, Game v);
void from_json(const nlohmann::json& j, Game& v);

void to_json(nlohmann::json& j, PokemonRegion v);
void from_json(const nlohmann::json& j, PokemonRegion& v);

void to_json(nlohmann::json& j, Language v);
void from_json(const nlohmann::json& j, Language& v);

void to_json(nlohmann::json& j, Condition v);
void from_json(const nlohmann::json& j, Condition& v);

void to_json(nlohmann::json& j, Theme v);
void from_json(const nlohmann::json& j, Theme& v);

}  // namespace ccm
