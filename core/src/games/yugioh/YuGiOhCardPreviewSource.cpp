#include "ccm/games/yugioh/YuGiOhCardPreviewSource.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <sstream>
#include <string>

namespace ccm {

namespace {

// RFC 3986 percent-encoder for the search-query payload. Same rules as the
// Magic implementation; kept private so the two can drift independently if a
// future API requires it.
std::string urlEncode(std::string_view in) {
    std::ostringstream out;
    out.fill('0');
    out << std::hex << std::uppercase;
    for (unsigned char c : in) {
        const bool unreserved =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~';
        if (unreserved) {
            out << static_cast<char>(c);
        } else {
            out << '%';
            out.width(2);
            out << static_cast<unsigned int>(c);
        }
    }
    return out.str();
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string imageFromCard(const nlohmann::json& card) {
    if (!card.contains("card_images") || !card.at("card_images").is_array() || card.at("card_images").empty()) {
        return {};
    }
    const auto& first = card.at("card_images").at(0);
    if (first.contains("image_url") && first.at("image_url").is_string()) {
        return first.at("image_url").get<std::string>();
    }
    if (first.contains("image_url_small") && first.at("image_url_small").is_string()) {
        return first.at("image_url_small").get<std::string>();
    }
    if (first.contains("image_url_cropped") && first.at("image_url_cropped").is_string()) {
        return first.at("image_url_cropped").get<std::string>();
    }
    return {};
}

}  // namespace

YuGiOhCardPreviewSource::YuGiOhCardPreviewSource(IHttpClient& http) : http_(http) {}

std::string YuGiOhCardPreviewSource::buildSearchUrl(std::string_view name, std::string_view setName) {
    std::string url =
        std::string("https://db.ygoprodeck.com/api/v7/cardinfo.php?fname=") + urlEncode(name);
    if (!setName.empty()) {
        url += "&cardset=";
        url += urlEncode(setName);
    }
    return url;
}

Result<std::string> YuGiOhCardPreviewSource::parseResponse(const std::string& body,
                                                           std::string_view setNo,
                                                           std::string_view setRarity) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return Result<std::string>::err("YGOPRODeck response missing 'data' array.");
        }
        const auto& data = j.at("data");
        if (data.empty()) {
            return Result<std::string>::err("YGOPRODeck returned no matching cards.");
        }

        const std::string wantedSetNo = trim(std::string(setNo));
        const std::string wantedRarity = trim(std::string(setRarity));

        for (const auto& card : data) {
            if (!card.contains("card_sets") || !card.at("card_sets").is_array()) continue;
            bool setNoMatch = wantedSetNo.empty();
            bool rarityMatch = wantedRarity.empty();
            for (const auto& print : card.at("card_sets")) {
                const std::string printCode = trim(print.value("set_code", ""));
                const std::string printRarity = trim(print.value("set_rarity", ""));
                if (!wantedSetNo.empty() && printCode == wantedSetNo) setNoMatch = true;
                if (!wantedRarity.empty() && printRarity == wantedRarity) rarityMatch = true;
            }
            if (setNoMatch && rarityMatch) {
                const std::string image = imageFromCard(card);
                if (!image.empty()) return Result<std::string>::ok(image);
            }
        }

        for (const auto& card : data) {
            if (!card.contains("card_sets") || !card.at("card_sets").is_array()) continue;
            for (const auto& print : card.at("card_sets")) {
                if (trim(print.value("set_code", "")) == wantedSetNo) {
                    const std::string image = imageFromCard(card);
                    if (!image.empty()) return Result<std::string>::ok(image);
                }
            }
        }

        const std::string image = imageFromCard(data.at(0));
        if (!image.empty()) {
            return Result<std::string>::ok(image);
        }
        return Result<std::string>::err("Card has no image variants.");
    } catch (const std::exception& e) {
        return Result<std::string>::err(
            std::string("YGOPRODeck JSON parse error: ") + e.what());
    }
}

Result<std::string> YuGiOhCardPreviewSource::fetchImageUrl(std::string_view name,
                                                           std::string_view setId,
                                                           std::string_view setNo) {
    std::string rarity;
    std::string number = std::string(setNo);
    const auto sep = number.find("||");
    if (sep != std::string::npos) {
        rarity = number.substr(sep + 2);
        number = number.substr(0, sep);
    }
    const std::string url = buildSearchUrl(name, setId);
    auto resp = http_.get(url);
    if (resp) {
        return parseResponse(resp.value(), number, rarity);
    }

    // Fallback: a strict/unknown set label can cause a 400 on cardset-filtered
    // requests. Retry with name-only fuzzy search and disambiguate locally.
    const std::string fallbackUrl = buildSearchUrl(name, "");
    auto fallback = http_.get(fallbackUrl);
    if (!fallback) return Result<std::string>::err(fallback.error());
    return parseResponse(fallback.value(), number, rarity);
}

}  // namespace ccm
