#pragma once

// Game / Language / Condition enums.
//
// String spellings are kept identical to the established Rust serde defaults
// (e.g. `NearMint`, `LightPlayed`, `Magic`, `Pokemon`) so existing JSON files
// remain interchangeable.

#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace ccm {

enum class Game {
    Magic,
    Pokemon,
};

enum class Language {
    English,
    German,
    French,
    Spanish,
    Italian,
    Chinese,
    Japanese,
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
std::string_view to_string(Language l) noexcept;
std::string_view to_string(Condition c) noexcept;
std::string_view to_string(Theme t) noexcept;

std::optional<Game>      gameFromString(std::string_view s) noexcept;
std::optional<Language>  languageFromString(std::string_view s) noexcept;
std::optional<Condition> conditionFromString(std::string_view s) noexcept;
std::optional<Theme>     themeFromString(std::string_view s) noexcept;

const std::array<Game, 2>&       allGames() noexcept;
const std::array<Language, 8>&   allLanguages() noexcept;
const std::array<Condition, 7>&  allConditions() noexcept;
const std::array<Theme, 2>&      allThemes() noexcept;

// nlohmann/json hooks - serialize as plain strings, matching Rust serde.
void to_json(nlohmann::json& j, Game v);
void from_json(const nlohmann::json& j, Game& v);

void to_json(nlohmann::json& j, Language v);
void from_json(const nlohmann::json& j, Language& v);

void to_json(nlohmann::json& j, Condition v);
void from_json(const nlohmann::json& j, Condition& v);

void to_json(nlohmann::json& j, Theme v);
void from_json(const nlohmann::json& j, Theme& v);

}  // namespace ccm
