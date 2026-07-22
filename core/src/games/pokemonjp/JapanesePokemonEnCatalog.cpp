#include "ccm/games/pokemonjp/JapanesePokemonEnCatalog.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace ccm {

namespace {

std::string asciiLower(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

std::string printKey(std::string_view setId, std::string_view localId) {
    return std::string(setId) + '\0' + std::string(localId);
}

bool isAsciiAlnumToken(std::string_view s) {
    if (s.empty()) return false;
    for (unsigned char ch : s) {
        if (!std::isalnum(ch)) return false;
    }
    return true;
}

/// True when `needle` appears in `hay` as a whole alphanumeric token
/// (e.g. "mewtwo" in "team gr's mewtwo" / "mewtwo strikes back", but not
/// "mew" inside "mewtwo"). ASCII needles only.
bool containsWholeAsciiToken(std::string_view hay, std::string_view needle) {
    if (!isAsciiAlnumToken(needle)) return false;
    const std::size_t n = needle.size();
    for (std::size_t i = 0; i + n <= hay.size(); ++i) {
        if (hay.compare(i, n, needle) != 0) continue;
        const bool leftOk = i == 0 || !std::isalnum(static_cast<unsigned char>(hay[i - 1]));
        const bool rightOk =
            i + n == hay.size() ||
            !std::isalnum(static_cast<unsigned char>(hay[i + n]));
        if (leftOk && rightOk) return true;
    }
    return false;
}

}  // namespace

Result<JapanesePokemonEnCatalog>
JapanesePokemonEnCatalog::parse(const std::string& jsonBody) {
    try {
        const auto j = nlohmann::json::parse(jsonBody);
        JapanesePokemonEnCatalog out;

        if (j.contains("sets") && j.at("sets").is_object()) {
            for (auto it = j.at("sets").begin(); it != j.at("sets").end(); ++it) {
                JapanesePokemonSetEnInfo info;
                info.nameEn = it.value().value("name_en", "");
                info.nameJa = it.value().value("name_ja", "");
                info.releaseDate = it.value().value("releaseDate", "");
                out.sets_[it.key()] = std::move(info);
            }
        }

        if (j.contains("prints") && j.at("prints").is_array()) {
            for (const auto& entry : j.at("prints")) {
                JapanesePokemonPrintEnInfo info;
                info.setId = entry.value("set_id", "");
                info.localId = entry.value("local_id", "");
                info.nameEn = entry.value("name_en", "");
                info.nameJa = entry.value("name_ja", "");
                info.nameEnSource = entry.value("name_en_source", "");
                info.imageUrl = entry.value("image_url", "");
                if (entry.contains("tcgplayer_id")) {
                    const auto& tp = entry.at("tcgplayer_id");
                    if (tp.is_string()) {
                        info.tcgplayerId = tp.get<std::string>();
                    } else if (tp.is_number_integer()) {
                        info.tcgplayerId = std::to_string(tp.get<std::int64_t>());
                    } else if (tp.is_number_unsigned()) {
                        info.tcgplayerId = std::to_string(tp.get<std::uint64_t>());
                    }
                }
                if (info.setId.empty() || info.localId.empty()) continue;
                const std::string key = printKey(info.setId, info.localId);
                out.printKeysBySet_[info.setId].push_back(key);
                out.printsByKey_[key] = std::move(info);
            }
        }

        return Result<JapanesePokemonEnCatalog>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<JapanesePokemonEnCatalog>::err(
            std::string("Japanese Pokemon EN catalog JSON parse error: ") + e.what());
    }
}

std::optional<JapanesePokemonSetEnInfo>
JapanesePokemonEnCatalog::findSet(std::string_view setId) const {
    const auto it = sets_.find(std::string(setId));
    if (it == sets_.end()) return std::nullopt;
    return it->second;
}

std::optional<JapanesePokemonPrintEnInfo>
JapanesePokemonEnCatalog::findPrint(std::string_view setId,
                                    std::string_view localId) const {
    const auto it = printsByKey_.find(printKey(setId, localId));
    if (it == printsByKey_.end()) return std::nullopt;
    return it->second;
}

std::vector<JapanesePokemonPrintEnInfo>
JapanesePokemonEnCatalog::findPrintsByName(std::string_view setId,
                                           std::string_view cardName) const {
    std::vector<JapanesePokemonPrintEnInfo> out;
    if (cardName.empty()) return out;
    const std::string wanted = asciiLower(std::string(cardName));
    const auto keysIt = printKeysBySet_.find(std::string(setId));
    if (keysIt == printKeysBySet_.end()) return out;
    for (const auto& key : keysIt->second) {
        const auto pit = printsByKey_.find(key);
        if (pit == printsByKey_.end()) continue;
        const auto& p = pit->second;
        const std::string enLower = asciiLower(p.nameEn);
        const std::string jaLower = asciiLower(p.nameJa);
        // Exact, qualified "Mewtwo (...)", or whole-token in a longer title
        // ("Team GR's Mewtwo", "Mewtwo Strikes Back (...)").
        if (enLower == wanted || jaLower == wanted ||
            enLower.starts_with(wanted + " (") ||
            containsWholeAsciiToken(enLower, wanted) ||
            containsWholeAsciiToken(jaLower, wanted)) {
            out.push_back(p);
        }
    }
    return out;
}

bool JapanesePokemonEnCatalog::hasPrintsForSet(std::string_view setId) const noexcept {
    const auto it = printKeysBySet_.find(std::string(setId));
    return it != printKeysBySet_.end() && !it->second.empty();
}

std::string JapanesePokemonEnCatalog::tcgplayerImageUrl(std::string_view productId) {
    if (productId.empty()) return {};
    return std::string("https://product-images.tcgplayer.com/fit-in/437x437/") +
           std::string(productId) + ".jpg";
}

std::string JapanesePokemonEnCatalog::previewImageUrlFromPrint(
    const JapanesePokemonPrintEnInfo& print) {
    if (!print.imageUrl.empty()) return print.imageUrl;
    return tcgplayerImageUrl(print.tcgplayerId);
}

}  // namespace ccm
